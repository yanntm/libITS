#ifndef __LABELED_NET_HH__
#define __LABELED_NET_HH__

#include "TypeBasics.hh"
#include "PNet.hh"
#include "TPNetSemantics.hpp"
#include <iosfwd>
#include <map>


namespace its {


/** Implement a type by adapting the PNet interface.
 *  Using delegation to build a type from a PNet */
template < typename  HomType >
class PNetTType :  public TypeBasics {
 protected :
  // the concrete storage class
  PNet net_;
  // used to label SDD node of PNet.
  static const int DEFAULT_VAR = 0;

  virtual labels_t getVarSet () const {
    labels_t pnames ;
    for (PNet::places_it it = net_.places_begin() ; it != net_.places_end(); ++it ) {
      pnames.push_back(it->getName());
    }
    labels_t pnames2;
//     for (labels_t::const_reverse_iterator rit = pnames.rbegin() ; rit != (const_cast<const labels_t &> (pnames)).rend() ; ++rit ) {
//       pnames2.push_back(*rit);
//     }
    return pnames;
  }

public :

  typedef  TPNetSemantics<HomType> Semantics ;

  PNetTType (const PNet & net) : net_(net) {};

  /** the set InitStates of designated initial states */
  labels_t getInitStates () const {
    labels_t labs;
    for (PNet::markings_it it = net_.markings_begin(); it != net_.markings_end() ; ++it) {
      labs.push_back(it->first);
    }
    return labs;
  }

  /** the set T of public transition labels */
  labels_t getTransLabels () const {
    labels_t labs;
    for ( PNet::trans_it it = net_.transitions_begin() ; it != net_.transitions_end(); ++it ) {
      if (it->getVisibility() == PUBLIC)
	labs.push_back(it->getName());
    }
    return labs;
  }

  virtual Transition getTransitionHom (const PTransition & t, const VarOrder & vo) const {
    return Semantics::encapsulate( Semantics::getFullHom(t, vo)) ;
  }

  /** state and transitions representation functions */
  /** Local transitions */
  Transition getLocals () const {
    std::set<GShom> locals;
    const VarOrder & vo = *getVarOrder();
    for ( PNet::trans_it it = net_.transitions_begin() ; it != net_.transitions_end(); ++it ) {
      if (it->getVisibility() == PRIVATE) {
	locals.insert( getTransitionHom (*it,vo) );
      }
    }
    if (! locals.empty())
      return GShom::add(locals);
    else
      return GShom(GSDD::null);
  }

  virtual HomType getTransitionEnabler (const PTransition & t, const VarOrder & vo) const {
    return  Semantics::getEnablerHom(t, vo) ;
  }

  virtual HomType getTransitionAction (const PTransition & t, const VarOrder & vo) const {
    return  Semantics::getActionHom(t, vo) ;
  }


  
  /** Successors synchronization function : Bag(T) -> SHom.
   * The collection represented by the iterator should be a multiset
   * of transition labels of this type (as obtained through getTransLabels()).
   * Otherwise, an assertion violation will be raised !!
   * */
  virtual Transition getSuccs (const labels_t & tau) const {
    // Build inh, pre, post for the compound transition 
    HomType enab= HomType::id;
    HomType action= HomType::id;
    // use the provided order
    const VarOrder * po =  getVarOrder();
    for (labels_t::const_iterator it = tau.begin() ; it != tau.end() ; ++it) {
      bool foundOne = false;
      HomType  enabler;
      HomType  actioner;
      for (PNet::trans_it t = net_.transitions_begin(); t != net_.transitions_end(); ++t ) {
	if (t->getLabel() == *it) {
	  if (!foundOne) {
	    enabler =  getTransitionEnabler(*t, *po);
	    actioner = getTransitionAction(*t, *po);
	    foundOne = true;
	  } else {
	    enabler =  enabler + getTransitionEnabler(*t, *po);
	    actioner = actioner + getTransitionAction(*t, *po);
	  }
	}
      }
      if (! foundOne) {
	std::cerr << "Asked for succ by transition "<< *it << " but no such transition label exists in type " << net_.getName() << std::endl ;
	assert(false);
      }
      // foo = \sum_i Compose ( foo_i ) 
      enab = enab & enabler;
      action = action & actioner;
    }
    // compound transition expression
    HomType trans = action & enab ; 
    // The DDD state variant uses an intermediate variable called DEFAULT_VAR
    Transition res = Semantics::encapsulate(trans);
    return res;
  }
	

  /** To obtain a representation of a labeled state */
  State getState(Label stateLabel) const {
    PNet::markings_it it = net_.markings_find(stateLabel);
    if (it == net_.markings_end()) {
      // DO NOT MISUSE : the state must have been defined
      assert (false) ;
      // for compiler warning : no return
      return State::null;
    } else {
      // an assignment style view of a marking
      const Marking & m = it->second;
      const VarOrder * vo = getVarOrder();
      
      return Semantics::getState (m, *vo);
    }
  }

  /** Use delegation on net_ */
  Label getName() const {
    return net_.getName();
  }

  std::ostream& print(std::ostream& os) const {
    return net_.print(os);
  }

  /** Print a set of states, explicitly. 
   *  Watch out, do not call on large its::State (>10^6) */
  void printState (State s, std::ostream & os) const {
    Semantics::printState (s, os, *getVarOrder());
  }



};


  typedef PNetTType<GHom> PNetType;
  typedef PNetTType<GShom> PNetSType;


}

template<typename HomType>
std::ostream & operator<< (std::ostream & os, const its::PNetTType<HomType> & net) {
  return net.print(os);
};


#endif
