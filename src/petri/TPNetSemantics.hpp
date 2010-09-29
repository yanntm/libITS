#ifndef __PTRANSITION__SEMANTICS__
#define __PTRANSITION__SEMANTICS__

#include "Hom.h"
#include "SHom.h"

#include "Hom_Basic.hh"
#include "Hom_PlaceArcs.hh"
#include "PTransition.hh"

namespace its {

  template <typename HomType> 
  class TPNetSemantics {
  public :
    typedef typename HomType::NodeType NodeType;
  
    // triggers foo(var,val) or localApply( foo(DEFAULT, val) , var) according to 
    // partial specialization used (see bottom of this file)
    static HomType getHom ( GHom (* foo) (int,int) , int var, int val) ;
  
    static HomType getEnablerHom (const Arc & a, const VarOrder & pi) {
      // to handle hyper arcs, we use a set and OR the result
      std::set<HomType> homs;
      if ( a.getType() == OUTPUT || a.getType() == RESET )
	return HomType::id;
      for (Arc::places_it it = a.begin(); it != a.end() ; ++it) {
	int val = it->getValuation();
	int pindex = pi.getIndex(it->getPlace());
	switch (a.getType()) {
	case INHIBITOR :
	  homs.insert( getHom ( varLeqState , pindex, val-1 ) );
	  break;
	case INPUT :
	case  TEST :
	  homs.insert( getHom ( varGtState , pindex, val-1 ) ); 
	  break;
	case OUTPUT :
	case RESET :
	  // no action needs to be taken
	  break;
	}
      }
      return HomType::add(homs);
    }
  
    static HomType getDisablerHom (const Arc & a, const VarOrder & pi) {
      return ! getEnablerHom(a,pi);
    }
  
    static HomType getActionHom (const Arc & a, const VarOrder & pi) {
      // to handle hyper arcs, we use a set and OR the result
      std::set<HomType> homs;
      if (a.getType() == INHIBITOR || a.getType() == TEST)
	return HomType::id;
      for (Arc::places_it it = a.begin(); it != a.end() ; ++it) {
	int val = it->getValuation();
	int pindex = pi.getIndex(it->getPlace());
	switch (a.getType()) {
	case INPUT :
	  // negative post arc ! no enabling test-set behavior
	  homs.insert( getHom ( postPlace, pindex, -val ));
	  break;
	case OUTPUT :
	  // the varGtState is introduced to help the invert relation be exact
	  homs.insert( getHom ( varGtState , pindex, 0 ) & getHom ( postPlace, pindex, val )); 
	  break;
	case RESET :
	  homs.insert( getHom ( setVarConst, pindex, 0) ); 
	  break;
	case TEST :
	case INHIBITOR :
	  // no action needs to be taken
	  return HomType::id;
	  break;
	}
      }
      return HomType::add(homs);
    }



    static HomType getActionHom (const PTransition & t, const VarOrder & pi) {
      HomType res;
      for (PTransition::arcs_it it = t.begin() ; it != t.end() ; ++it ) {
	res = getActionHom(*it,pi) & res ;
      }
      return res;
    }
    
    static HomType getEnablerHom (const PTransition & t, const VarOrder & pi) {
      HomType res;
      for (PTransition::arcs_it it = t.begin() ; it != t.end() ; ++it ) {
	res = getEnablerHom(*it,pi) & res ;
      }
      return res;
    }

    static HomType getFullHom (const PTransition & t, const VarOrder & pi) {  
      return getActionHom(t,pi) & getEnablerHom(t,pi);
    }

    static HomType getOK (const TimeConstraint & clock, int cvar) {
      HomType h = HomType::id;
      if ( clock.lft == 0) {
	// [0,0] clocks are always ok ...
	return HomType::id;
      }
      if ( clock.lft != -1 )
	// not an infinite lft
	h = getHom ( varLeqState , cvar, clock.lft ) ;
      if ( clock.eft > 0 ) 
	// not earliest firing time = 0
	h = getHom ( varGtState, cvar, clock.eft - 1) & h;
      return h;
    }

    static HomType getRZ (const TimeConstraint & clock, int cvar)  {
      if (clock.lft == 0)
	// [0,0] clocks are never incremented... no need to reset
	return HomType::id;
      return getHom ( setVarConst, cvar, 0 ) ;
    }    

    static HomType getIncr (const TimeConstraint & clock, int cvar)  {
      if ( clock.lft == -1 ) {
	if (clock.eft == 0) {
	  // [0, INF] clock
	  return HomType::id;
	} else {
	  // [eft,INF] case
	  // if lft = INF, only increment up to eft, then leave at value
	  return  ITE( getHom(varGtState, cvar, clock.eft-1), HomType::id,  getHom( postPlace,cvar,1)) ;
	}
      } else if (clock.lft == 0) {
	// [0,0] bounds
	return NodeType::null;
      }
      // [x, y] general case, x >= 0 and y < INF
      return getHom(postPlace,cvar,1) & getHom(varLeqState, cvar, clock.lft - 1);
    }
    
    static NodeType newNode (int var, int val) ;

    static NodeType getMarking (const Marking &m, const VarOrder & vo) {
      // converting to DDD first
      NodeType M0 = NodeType::one;
      // each place = one var as indicated by getPorder
      for (size_t i=0 ; i < vo.size() ; ++i) {
	Label pname = vo.getLabel(i);
	// retrieve the appropriate place marking
	int mark = m.getMarking(pname);
	// left concatenate to M0
	M0 = newNode (i,mark) ^ M0;
	// for pretty print
	DDD::varName(i,pname);
      }
      return M0; 
    }
    
    static GSDD getState (const Marking &m, const VarOrder & vo) ;

    static GShom encapsulate (const HomType & h);

    static void printState (State s, std::ostream & os, const VarOrder & vo) ;
    
  };

  // user manipulated types
  typedef TPNetSemantics<GHom> dddSemantics;
  typedef TPNetSemantics<GShom> sddSemantics;

  // partial specializations : transitions

  template <>
  inline GHom dddSemantics::getHom (GHom (* foo) (int,int) , int var, int val) {
    return foo(var,val);
  }
  
  template <>
  inline GShom sddSemantics::getHom (GHom (* foo) (int,int) , int var, int val) {
    return localApply( foo(DEFAULT_VAR,val) , var );
  }

  template <>
  inline GShom dddSemantics::encapsulate (const GHom & h) {
    return localApply ( h, DEFAULT_VAR);
  }

  template <>
  inline GShom sddSemantics::encapsulate (const GShom & h) {
    return h;
  }

  // partial specialization : nodes 
  template <>
  inline GSDD sddSemantics::newNode (int var, int val) {
    return GSDD (var, DDD(DEFAULT_VAR, val) );
  }

  template <>
  inline GDDD dddSemantics::newNode (int var, int val) {
    return GDDD (var, val) ;
  }

  template <>
  inline GSDD dddSemantics::getState (const Marking &m, const VarOrder & vo) {
    return GSDD( DEFAULT_VAR, DDD(getMarking(m,vo)));
  }

  template <>
  inline GSDD sddSemantics::getState (const Marking &m, const VarOrder & vo) {
    return getMarking(m,vo);
  }



  static void recPrintDDD (const GDDD & d, std::ostream & os, const VarOrder & vo, vLabel str) {
    if (d == DDD::one)
      os << "[ " << str << "]"<<std::endl;
    else if(d == DDD::top)
      os << "[ " << str << "T ]"<<std::endl;
    else if(d == DDD::null)
      os << "[ " << str << "0 ]"<<std::endl;
    else{
      if (d.nbsons() == 1 && d.begin()->first == 0) {
	recPrintDDD(d.begin()->second,os,vo,str);
      } else {
	for(GDDD::const_iterator vi=d.begin();vi!=d.end();++vi){
	  std::stringstream tmp;
	  tmp << vo.getLabel(d.variable())<<'('<<vi->first<<") ";
	  recPrintDDD(vi->second,os,vo,str+tmp.str());
	}
      }
    }
  }

  static void recPrintSDD (State s, std::ostream & os, const VarOrder & vo, vLabel str) {
    if (s == State::one)
      os << "[ " << str << "]";
    else if(s ==  State::top)
      os << "[ " << str << "T ]";
    else if(s == State::null)
      os << "[ " << str << "0 ]";
    else{
      for(State::const_iterator vi=s.begin(); vi!=s.end(); ++vi){
	
		
	// grab the DDD on the arc	
	DDD val = (const DDD &) * vi->first;
	
	if (val.nbsons() == 1 && val.begin()->first == 0) {
	  // skip {0} values
	  recPrintSDD(vi->second, os, vo, str);
	} else {
	  std::stringstream tmp;
	  // pretty print variable names
	  Label varname = vo.getLabel(s.variable());
	  tmp << varname << "={";
	
	  for (DDD::const_iterator it = val.begin(); it != val.end() ; /**increment in loop */) {
	    tmp << to_string(it->first) ;
	    ++it;
	    if (it != val.end()) tmp << ",";
	  }
	  tmp << "} ";
	
	  recPrintSDD(vi->second, os, vo, str + tmp.str());
	}
      }
    }
  }

  template<>
  inline void sddSemantics::printState (State s, std::ostream & os, const VarOrder & vo) {
    recPrintSDD(s, os, vo, "");
  }

  template<>
  inline void dddSemantics::printState (State s, std::ostream & os, const VarOrder & vo) {
    // should have a single variable, hence a single arc with a DDD label
    assert(s.begin() != s.end());
    assert(s.begin()->second == State::one);
    DDD state = (const DDD &) * s.begin()->first;
    // for now just invoke DDD print
    recPrintDDD(state, os, vo, "");
  }


} // namespace


#endif
