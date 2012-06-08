#include "IntExpression.hh"
#include "BoolExpression.hh"

#include <cmath>
#include <cassert>
#include "hashfunc.hh"
#include <typeinfo>


namespace its {

  /** TOC : 
      1. environment labels_t operations
      2. IntExpression
      3. BoolExpression */


  /******************************* THIS SECTION : (inline) utilities for environment manipulation  ************************/

  typedef PIntExpression::indexes_t indexes_t;



  std::ostream & operator<< (std::ostream & os, const labels_t & env) {
    if (! env.empty()) {
      for (labels_it it = env.begin() ; /* in loop */ ; /* in loop */ ) {
	os << *it ;
	++it;
	if (it != env.end())
	  os << ", ";
	else
	  break;
      }
    }
    return os;
  }


  static inline int indexOf (Label name, const labels_it & begin, const labels_it & end) {
      int index = 0;
      for (labels_it it = begin;
	   it != end;
	   ++it,++index ) {
	if (*it == name) {
	  return index;
	}
      }
      return -1;
  }

  // True if "a" is a superset of "b"
  static bool supersets ( const labels_t & a, const labels_t & b) {
    labels_it pa = a.begin();
    labels_it pb = b.begin();
    labels_it aend = a.end();
    labels_it bend = b.end();

    for ( ; pa !=  aend && pb != bend ; ) {
      if (pa == aend) {
	// b not yet empty !
	return false;
      } else if (pb == bend) {
	// b exhausted, a is a superset
	return true;
      } else if (*pa == *pb) {
	++pa;
	++pb;
      } else if (*pa > *pb) {
	// can't matchthis b elt in a
	return false;
      } else {
	// next a elt
	++pa;
      }
    }
    // a and b were equal
    return true;
  }

  static inline labels_t sorted_union ( const labels_t & a, const labels_t & b) {
    labels_t unione;
    labels_it pa = a.begin();
    labels_it pb = b.begin();
    labels_it aend = a.end();
    labels_it bend = b.end();

    // Quit when both a and b are exhausted
    while ( pa != aend || pb != bend ) {
      // at end of a => push b onto union
      // else if b not empty and is smaller push b onto union
      if ( pa == aend || ( pb != bend && *pb < *pa ) ) {
	unione.push_back(*pb);
	++pb;
      
      // at end of b => push a onto union
      // else (a is not empty tested above) push a if it is smaller	
      } else if ( pb == bend || *pa < *pb ) {
	unione.push_back(*pa);
	++pa; 

      // Case is only reached if a and b non empty and ( *pa == *pb ) 	
      } else { 
	unione.push_back(*pa);
	++pa;
	++pb;
      } 
    }
    return unione;
  }



  static inline indexes_t reindex ( const labels_t & oldenv, const labels_t & newenv, bool * shouldRewrite ) {
    indexes_t res ( oldenv.size() , 0);
    int count = 0;
    for (labels_it it = oldenv.begin() ; it != oldenv.end() ; ++it, ++count ) {
      int i = indexOf(*it,newenv.begin(), newenv.end());
      if (i == -1) {
	std::cerr << "Error : While reindexing : new environment is not a superset of old one" << std::endl;
	std::cerr << "was looking for  " << *it << " in environment " << newenv << std::endl ;
      }

      res [count] = i;
      if (count != i) {
	*shouldRewrite = true ;
      }
    }
    return res;
  }


  /** shrinks environment to fit expression. If needed cleanup holes and reindex expression. */
  template <typename T>
  static inline std::pair<T,labels_t> gc (const T & expr, const labels_t env) {
    typedef std::pair<T, labels_t> exprenv_t;
    // prepare new (possibly reduced environment
    size_t oldsize = env.size();
    // for gc : unmarked entries will be cleared
    bool mark [oldsize];
    for (size_t i =0 ; i < oldsize ; ++i)
      mark[i] = false;

    // mark as appropriate by traversing the expression
    expr.getSupport(mark);
    
    // build new environment and permutation
    exprenv_t toret (expr, labels_t());
    indexes_t perm;
    size_t pos = 0;
    bool shouldreindex;
    
    for (size_t i = 0 ; i < oldsize ; ++i) {
      // label is kept
      if (mark[i]) {
	toret.second.push_back(env[i]);
	if (i != pos)
	  shouldreindex = true;
	perm.push_back(pos++);
      } else {
	perm.push_back(-1);
      }
    }

    // if should reindex, update expression
    if (shouldreindex)
      toret.first = toret.first.reindexVariables(perm);

    return toret;
  }


template <typename Expr, typename PExpr>
static inline PExpr normalize (const PExpr & expr, const labels_t & env, const labels_t & unione) {
  // to store whether reindex is necessary
  bool rwl=false;
  // compute possible reindexing of formulas in l.expr
  // eg. reindexl = {"a"}.rw({"a","b"}) = [ 0 , 1 ], and rwl=false since "a" did not move
  indexes_t reindexl = reindex ( env, unione, &rwl);
  
  PExpr x0 = expr;
  // eg. avoid this unecessary reindex
  if (rwl) {
    // eg. would not have any effect anyway
    x0 = x0.reindexVariables(reindexl);
  }
  
  return x0;
}

template <typename Expr, typename PExpr>
static inline PExpr normalize (const Expr & expr, const labels_t & unione) {
  
  return normalize<Expr,PExpr> (expr.getExpr(), expr.getEnv(), unione);
}


  /******************************* THIS SECTION : INT EXPR ************************/



// unique storage class
class _IntExpression {
  // count references
  mutable size_t refCount;
  // access to refcount for garbage purpose
  friend class IntExpressionFactory;
  // The actual 'formal' expression with parameters
  PIntExpression expr;
  // The parameters expansion to variable names
  typedef labels_t env_t;
  env_t env;

 public :

  const env_t & getEnv() const { return env ; }
  const PIntExpression & getExpr() const { return expr ; }


  // add a ref
  size_t ref () const { 
    return ++refCount;
  }
  // dereference
  size_t deref () const { 
    assert(refCount >0);
    return --refCount;
  }
  // default constructor
  _IntExpression (const PIntExpression & expr, const labels_t & env): refCount(0),expr(expr),env(env) {}
  // reclaim memory
  virtual ~_IntExpression () { assert (refCount==0); };
  
  ///////// Interface functions
  // for hash storage
  size_t hash () const {
    size_t toret = 103843;
    for (labels_it it = env.begin() ; it != env.end() ; ++it ) {
      toret ^= d3::util::hash<std::string> () (*it)+ 103399;
    }
    return toret ^ expr.hash();
  }

  bool operator==(const _IntExpression & e) const {
    return env == e.env && expr.equals(e.expr) ;
  }
  _IntExpression * clone () const {
    return new _IntExpression(*this);
  }

  // to avoid excessive typeid RTTI calls.
  IntExprType getType() const {
    return expr.getType();
  }

  // pretty print
  void print (std::ostream & os) const {
    expr.print(os,env);
//     os << "{"  << env;
//     os << "|> " << expr << "}";
  }

  // Evaluate an expression.
  IntExpression eval() const {

    // simplify expression
    PIntExpression newexpr = expr.eval();

    // early exit if possible
    if (newexpr.equals(expr)) {
      return this;
    }

    std::pair<PIntExpression, labels_t> aftergc = gc ( newexpr, env );

    return IntExpressionFactory::createUnique(_IntExpression(aftergc.first, aftergc.second ));
  }

  IntExpression setAssertion (const Assertion & a) const {
    // break out if "a" references variables we don't know about => no possible simplifications.
    if (! supersets(env, a.getEnv())) {
      return this;
    }

    labels_t unione = sorted_union (a.getEnv() , env );

    PIntExpression ex = normalize<IntExpression,PIntExpression> (expr, env, unione);

    PIntExpression aa = normalize<IntExpression,PIntExpression> (a.getAssertion().getFirst(), a.getEnv() , unione);
    PIntExpression aaprime = normalize<IntExpression,PIntExpression> (a.getAssertion().getSecond(), a.getEnv() , unione);

    ex = ex & PIntExpressionFactory::createPAssertion(aa,aaprime);

    std::pair<PIntExpression, labels_t> aftergc = gc ( ex, unione );

    return IntExpressionFactory::createUnique(_IntExpression(aftergc.first, aftergc.second ));
  }

  bool isSupport (const Variable & v) const {
    int id = indexOf(v.getArrayName(),env.begin(),env.end());
    if (id==-1)
      return false;
    return expr.isSupport( id, v.getIndex() ) ;
  }

  std::set<Variable> getSupport() const {
    return std::set<Variable> (env.begin(), env.end());
  }

  IntExpression getFirstSubExpr () const {
    PIntExpression newexpr = expr.getFirstSubExpr();
    if ( expr.equals(newexpr) ) {
      return this;
    } 
    std::pair<PIntExpression, labels_t> aftergc = gc ( newexpr, env );

    return IntExpressionFactory::createUnique(_IntExpression(aftergc.first, aftergc.second ));    
  }

  IntExpression getSubExprExcept (const IntExpression & var) const {
    labels_t unione = sorted_union (var.getEnv() , env );

    PIntExpression varx = normalize<IntExpression,PIntExpression> (var, unione);
    
    
    PIntExpression newexpr = 0;
    if (varx.getType() == VAR ) {
      newexpr = expr.getSubExprExcept(varx.getVariable() , -1);
    } else if (varx.getType() == CONSTARRAY ) {
      newexpr = expr.getSubExprExcept(varx.getVariable() , varx.getValue());
    } else {
      std::cerr << "Unexpected target not of type VAR or CONSTARRAY within getSubExprExcept " << std::endl;
      assert(false);
    }

    if ( expr.equals(newexpr) ) {
      // Given the conditions for calling this function, we expect that :
      // The expression has a support that includes varx, but is not limited to varx.
      // Hence, te expression is not a constant
      std::cerr << "In getSubExprExcept(" ;  var.print(std::cerr) ;  std::cerr << ") for expression " ;
      this->print(std::cerr); std::cerr << " Unexpected return value." << std::endl;
//      std::cerr << "this.isSupport(var) = " << this->isSupport(var) << std::endl;

      assert(false);
    }

    std::pair<PIntExpression, labels_t> aftergc = gc ( newexpr, unione );

    return IntExpressionFactory::createUnique(_IntExpression(aftergc.first, aftergc.second ));    
  }

};



/********************************************************/
/***********  Assertion *********************************/

  Assertion::Assertion (const IntExpression & var, const IntExpression & val) : assertion(0,0){
  env = sorted_union (var.getEnv(), val.getEnv());

  
  assertion = PIntExpressionFactory::createPAssertion(
		      normalize<IntExpression,PIntExpression> (var,env),
		      normalize<IntExpression,PIntExpression> (val,env));

}

size_t Assertion::hash() const {
  size_t toret = 37459 ^ assertion.hash();
  for (labels_it it = env.begin() ; it != env.end() ; ++it ) {
    toret ^= d3::util::hash<std::string> () (*it) + 12397;
  }
  return toret ;

}

bool Assertion::operator== (const Assertion & other) const {
  return env == other.env && assertion == other.assertion;
}
  
bool Assertion::operator< (const Assertion & other) const {
  return env < other.env ? 
	       true : (
		       (env == other.env) ? 
		       assertion < other.assertion :
		       false );
}

Assertion Assertion::operator & (const Assertion & other) const {
  labels_t unione = sorted_union (env, other.env);

  PIntExpression a = normalize<IntExpression,PIntExpression> (assertion.getFirst(), env, unione);
  PIntExpression aprime = normalize<IntExpression,PIntExpression> (assertion.getSecond(), env, unione);

  PIntExpression b = normalize<IntExpression,PIntExpression> (other.assertion.getFirst(), other.env, unione);
  PIntExpression bprime = normalize<IntExpression,PIntExpression> (other.assertion.getSecond(), other.env, unione);

  return IntExpressionFactory::createAssertion (
	    PIntExpressionFactory::createPAssertion(a,aprime) 
	    & PIntExpressionFactory::createPAssertion (b,bprime)
	    , unione);
}

/// To determine whether a given variable is mentioned in an expression.
bool Assertion::isSupport (const Variable & v) const {
  int id = indexOf(v.getArrayName(),env.begin(),env.end());
  if (id==-1)
    return false;
  return assertion.isSupport(id, v.getIndex());
}

void Assertion::print (std::ostream & os) const {
  assertion.print(os,env) ;
}

std::ostream & operator<< (std::ostream & os, const Assertion & a) {
    a.print(os);
    return os;
}



/*******************************************************/
/******* Factory ***************************************/
// namespace IntExpressionFactory {


  // return the supporting parametric expression
  const class PIntExpression & IntExpression::getExpr() const { return concrete->getExpr(); }
  // return the environment
  const labels_t & IntExpression::getEnv() const { return concrete->getEnv() ; }




UniqueTable<_IntExpression>  IntExpressionFactory::unique = UniqueTable<_IntExpression>();



IntExpression IntExpressionFactory::createNary (IntExprType type, const NaryParamType & params) {
  
  // build the union of both envs, sorted on var names
  // eg. unione = {"a","b"}
  labels_t unione ;
  for (NaryParamType::const_iterator it = params.begin(); it != params.end(); ++it ){
    unione = sorted_union (unione, it->concrete->getEnv() );
  }
  
  NaryPParamType pparams;
  for (NaryParamType::const_iterator it = params.begin(); it != params.end(); ++it ){
    pparams.insert(normalize<IntExpression, PIntExpression> (*it, unione));
  }
    
  return createUnique(_IntExpression(PIntExpressionFactory::createNary (type, pparams),unione));
}


IntExpression IntExpressionFactory::createBinary (IntExprType type, const IntExpression & l, const IntExpression & r) {
  // eg. Running example for comments : type=ADD, l=<expr=x0;env={"a"}>, r=<expr=x0;env={"b"}>

  // build the union of both envs, sorted on var names
  // eg. unione = {"a","b"}
  labels_t unione = sorted_union ( l.concrete->getEnv(), r.concrete->getEnv());
  
  // now that the alphabets are compatible, build a parametric expression
  // e.g.   res = Add(Var(x0), Var(x1))
  PIntExpression res = PIntExpressionFactory::createBinary (type, normalize<IntExpression,PIntExpression>(l,unione) , normalize<IntExpression,PIntExpression>(r,unione)) ;

  return createUnique(_IntExpression(res, unione));
}

IntExpression IntExpressionFactory::createConstant (int v) {
  return createUnique (_IntExpression(PIntExpressionFactory::createConstant(v),labels_t()));
}

IntExpression IntExpressionFactory::createVariable (const Variable & v) {
  if (v.getArrayName() != v.getName()) {
    return createArrayAccess(v.getArrayName(), v.getIndex());
  }
  return createUnique (_IntExpression(PIntExpressionFactory::createVariable(0),labels_t(1,v.getName())));
}


IntExpression IntExpressionFactory::createArrayAccess (const Variable & v, const IntExpression & index) {
  IntExpression var = createVariable (v);

  // build the union of both envs, sorted on var names
  // eg. unione = {"a","b"}
  labels_t unione = sorted_union ( var.getEnv(), index.getEnv());

  
  // now that the alphabets are compatible, build a parametric expression
  // e.g.   res = Add(Var(x0), Var(x1))
  PIntExpression res = PIntExpressionFactory::createArrayAccess (normalize<IntExpression,PIntExpression>(var,unione).getVariable(),  normalize<IntExpression,PIntExpression>(index,unione)) ;


  return createUnique(_IntExpression(res, unione));
}

IntExpression IntExpressionFactory::wrapBoolExpr (const BoolExpression &b) {
  return createUnique (_IntExpression(PIntExpressionFactory::wrapBoolExpr(b.getExpr()),b.getEnv()));
}

IntExpression IntExpressionFactory::createIntExpression (const PIntExpression & pie, const labels_t & env) {
  return createUnique( _IntExpression(pie,env) );
}

Assertion IntExpressionFactory::createAssertion (const Variable & v,const IntExpression & e) {
  return createAssertion (createVariable(v),e);
}

Assertion IntExpressionFactory::createAssertion (const PAssertion & a,const labels_t & env) {
  return Assertion(a,env);
}


Assertion IntExpressionFactory::createAssertion (const IntExpression & v,const IntExpression & e) {
  return Assertion(v,e);
}

const _IntExpression * IntExpressionFactory::createUnique(const _IntExpression &e) {
  return unique(e);
}

void IntExpressionFactory::destroy (_IntExpression * e) {
  if (  e->deref() == 0 ){
    UniqueTable<_IntExpression>::Table::iterator ci = unique.table.find(e);
    assert (ci != unique.table.end());
    unique.table.erase(ci);
    delete e;
  }
}

void IntExpressionFactory::printStats (std::ostream &os) {
  os << "Integer expression entries :" << unique.size() << std::endl;
}

// } end namespace IntExpressionFactory


// namespace IntExpression {
// friend operator
IntExpression operator+(const IntExpression & l,const IntExpression & r) {  
  return IntExpressionFactory::createBinary (PLUS, l, r);
} 

IntExpression operator*(const IntExpression & l,const IntExpression & r) {  
  return IntExpressionFactory::createBinary (MULT, l, r);
} 



// necessary administrative trivia
// refcounting
IntExpression::IntExpression (const _IntExpression * concret): concrete(concret) {
  concrete->ref();
}

IntExpression::IntExpression (const IntExpression & other) {
  if (this != &other) {
    concrete = other.concrete;
    concrete->ref();
  }
}


IntExpression::IntExpression (int cst) {
  concrete = IntExpressionFactory::createUnique(_IntExpression(PIntExpressionFactory::createConstant(cst), labels_t()));
  concrete->ref();
}

IntExpression::IntExpression (const Variable & var) {
  concrete = IntExpressionFactory::createUnique(_IntExpression(PIntExpressionFactory::createVariable(0), labels_t(1, var.getName())));
  concrete->ref();
}

IntExpression::~IntExpression () {
  // remove const qualifier for delete call
  IntExpressionFactory::destroy((_IntExpression *) concrete);  
}

IntExpression & IntExpression::operator= (const IntExpression & other) {
  if (this != &other) {
    // remove const qualifier for delete call
    IntExpressionFactory::destroy((_IntExpression *) concrete);
    concrete = other.concrete;
    concrete->ref();
  }
  return *this;
}

bool IntExpression::equals (const IntExpression & other) const {
  return concrete == other.concrete ;
}

bool IntExpression::less (const IntExpression & other) const {
  return concrete < other.concrete;
}

IntExpression IntExpression::getFirstSubExpr () const {
  return concrete->getFirstSubExpr();
}

IntExpression IntExpression::getSubExprExcept  (const IntExpression & v) const {
  return concrete->getSubExprExcept(v);
}



void IntExpression::print (std::ostream & os) const {
  concrete->print(os);
}

IntExpression IntExpression::eval () const {
  return concrete->eval();
}

/// only valid for CONST expressions
/// use this call only in form : if (e.getType() == CONST) { int j = e.getValue() ; ...etc }
/// Exceptions will be thrown otherwise.
int IntExpression::getValue () const { 
  return  concrete->getExpr().getValue();
}


vLabel IntExpression::getName () const {
  if (getType() == VAR) {
    return concrete->getEnv() [concrete->getExpr().getVariable() ];    
  } else if (getType() == CONSTARRAY) {
    return concrete->getEnv() [concrete->getExpr().getVariable() ] + "[" + to_string(concrete->getExpr().getValue()) + "]" ;    
  } else {
    std::cerr << "Calling getName on an a expression : " << *this << std::endl;
    throw "Do not call getName on non Variable type int expressions.";
  }

}


bool IntExpression::isSupport(const Variable & var) const {
  return concrete->isSupport(var);
}

std::set<Variable> IntExpression::getSupport() const {
  return concrete->getSupport();
}

IntExpression IntExpression::operator& (const Assertion &a) const {
  return concrete->setAssertion(a);
}

IntExprType IntExpression::getType() const {
  return concrete->getType();
}
// binary
IntExpression operator-(const IntExpression & l,const IntExpression & r)  {
  return IntExpressionFactory::createBinary(MINUS,l,r);
}
// unary -
IntExpression operator-(const IntExpression & l)  {
  return IntExpressionFactory::createBinary(MINUS,  IntExpressionFactory::createConstant(0),l);
}


IntExpression operator/(const IntExpression & l,const IntExpression & r) {
  return IntExpressionFactory::createBinary(DIV,l,r);
}
IntExpression operator%(const IntExpression & l,const IntExpression & r) {
  return IntExpressionFactory::createBinary(MOD,l,r);
}
IntExpression operator^(const IntExpression & l,const IntExpression & r) {
  return IntExpressionFactory::createBinary(POW,l,r);
}

size_t IntExpression::hash () const { 
  return ddd::knuth32_hash(reinterpret_cast<const size_t>(concrete)); 
}

std::ostream & operator << (std::ostream & os, const its::IntExpression & e) {
  e.print(os);
  return os;
}



  /******************************* THIS SECTION : BOOL EXPR ************************/
  
  // unique storage class
class _BoolExpression {
  // count references
  mutable size_t refCount;
  // access to refcount for garbage purpose
  friend class BoolExpressionFactory;
  // The actual 'formal' expression with parameters
  PBoolExpression expr;
  // The parameters expansion to variable names
  labels_t env;

public : 
  // add a ref
  size_t ref () const { 
    return ++refCount;
  }
  // dereference
  size_t deref () const { 
    assert(refCount >0);
    return --refCount;
  }

  // default constructor
  _BoolExpression (const PBoolExpression & expr, const labels_t & env): refCount(0),expr(expr),env(env) {}

  // reclaim memory
  ~_BoolExpression () { assert (refCount==0); };
 
  const labels_t & getEnv() const { return env ; }
  const PBoolExpression & getExpr() const { return expr ; }
  
  ///////// Interface functions
  // for hash storage
  size_t hash () const {
    size_t toret = 104459 ^ expr.hash();
    for (labels_it it = env.begin() ; it != env.end() ; ++it ) {
      toret ^= d3::util::hash<std::string> () (*it) + 102397;
    }
    return toret ;
  }

  bool operator==(const _BoolExpression & e) const {
    return env == e.env && expr == e.expr ;
  }

  _BoolExpression * clone () const {
    return new _BoolExpression(*this);
  }

  // to avoid excessive typeid RTTI calls.
  BoolExprType getType() const {
    return expr.getType();
  }

   // pretty print
  void print (std::ostream & os) const {
    expr.print(os,env);
//     os << "{" ;
//     os << env;
//     os << "|> " << expr << "}";
  }

    // Evaluate an expression.
  BoolExpression eval() const {

    // simplify expression
    PBoolExpression newexpr = expr.eval();

    // early exit if possible
    if (newexpr == expr) {
      return this;
    }

    std::pair<PBoolExpression, labels_t> aftergc = gc ( newexpr, env );

    return BoolExpressionFactory::createUnique(_BoolExpression(aftergc.first, aftergc.second ));
  }

  BoolExpression setAssertion (const Assertion & a) const {
    // break out if "a" references variables we don't know about => no possible simplifications.
    if (! supersets(env, a.getEnv())) {
      return this;
    }


    labels_t unione = sorted_union (a.getEnv() , env );

    PBoolExpression ex = normalize<BoolExpression,PBoolExpression> (expr, env, unione);

    PIntExpression aa = normalize<IntExpression,PIntExpression> (a.getAssertion().getFirst(), a.getEnv() , unione);
    PIntExpression aaprime = normalize<IntExpression,PIntExpression> (a.getAssertion().getSecond(), a.getEnv() , unione);

    ex = ex & PIntExpressionFactory::createPAssertion(aa,aaprime);

    std::pair<PBoolExpression, labels_t> aftergc = gc ( ex, unione );

    return BoolExpressionFactory::createUnique(_BoolExpression(aftergc.first, aftergc.second ));
  }

  

  bool isSupport (const Variable & v) const {
    int id = indexOf(v.getArrayName(),env.begin(),env.end());
    if (id==-1)
      return false;
    return expr.isSupport(id, v.getIndex());
  }

  std::set<Variable> getSupport() const {
    return std::set<Variable> (env.begin(), env.end());
  }
  
   IntExpression getFirstSubExpr () const {
    PIntExpression newexpr = expr.getFirstSubExpr();
    std::pair<PIntExpression, labels_t> aftergc = gc ( newexpr, env );

    return IntExpressionFactory::createUnique(_IntExpression(aftergc.first, aftergc.second ));    
  }
  

}; // class bool expr




UniqueTable<_BoolExpression>  BoolExpressionFactory::unique = UniqueTable<_BoolExpression>();

BoolExpression BoolExpressionFactory::createNary (BoolExprType type, const NaryBoolParamType & params) {
  
  // build the union of both envs, sorted on var names
  // eg. unione = {"a","b"}
  labels_t unione ;
  for (NaryBoolParamType::const_iterator it = params.begin(); it != params.end(); ++it ){
    unione = sorted_union (unione, it->concrete->getEnv() );
  }
  
  NaryPBoolParamType pparams;
  for (NaryBoolParamType::const_iterator it = params.begin(); it != params.end(); ++it ){
    pparams.insert(normalize<BoolExpression,PBoolExpression>(*it, unione));
  }
    
  return createUnique(_BoolExpression(PBoolExpressionFactory::createNary (type, pparams),unione));
}

BoolExpression BoolExpressionFactory::createBinary (BoolExprType type, const BoolExpression & l, const BoolExpression & r) {
  labels_t unione = sorted_union ( l.concrete->getEnv(), r.concrete->getEnv());

  // now that the alphabets are compatible, build a parametric expression
  // e.g.   res = Add(Var(x0), Var(x1))
  PBoolExpression res = PBoolExpressionFactory::createBinary (type
							      , 
							      normalize<BoolExpression,PBoolExpression>(l,unione) 
							      , 
							      normalize<BoolExpression,PBoolExpression>(r,unione)) ;

  return createUnique(_BoolExpression(res, unione));
}


BoolExpression BoolExpressionFactory::createNot  (const BoolExpression & e) {
  return createUnique(_BoolExpression(PBoolExpressionFactory::createNot(e.getExpr()),e.getEnv()));
}


BoolExpression BoolExpressionFactory::createConstant (bool b) {
  return createUnique(_BoolExpression(PBoolExpressionFactory::createConstant(b),labels_t()));
}

// a comparison (==,!=,<,>,<=,>=) between two integer expressions
BoolExpression BoolExpressionFactory::createComparison (BoolExprType type, const IntExpression & l, const IntExpression & r) {
  labels_t unione = sorted_union ( l.concrete->getEnv(), r.concrete->getEnv());

  // now that the alphabets are compatible, build a parametric expression
  // e.g.   res = Add(Var(x0), Var(x1))
  PBoolExpression res = PBoolExpressionFactory::createComparison (type, normalize<IntExpression,PIntExpression>(l,unione) , normalize<IntExpression,PIntExpression>(r,unione)) ;

  return createUnique(_BoolExpression(res, unione));
}


void BoolExpressionFactory::destroy (_BoolExpression * e) {
  if (  e->deref() == 0 ){
    UniqueTable<_BoolExpression>::Table::iterator ci = unique.table.find(e);
    assert (ci != unique.table.end());
    unique.table.erase(ci);
    delete e;
  }
}

const _BoolExpression * BoolExpressionFactory::createUnique(const _BoolExpression &e) {
  return unique(e);
}



void BoolExpressionFactory::printStats (std::ostream &os) {
  os << "Boolean expression entries :" << unique.size() << std::endl;
}

// nary constructions
BoolExpression operator&&(const BoolExpression & l,const BoolExpression & r) {
 return BoolExpressionFactory::createBinary (AND, l, r);
} 
BoolExpression operator||(const BoolExpression & l,const BoolExpression & r) {
 return BoolExpressionFactory::createBinary (OR, l, r);
}

BoolExpression BoolExpression::operator!() const {
  return BoolExpressionFactory::createNot(*this);
} 

BoolExpression::BoolExpression (const IntExpression & expr): concrete(NULL) {
   *this = BoolExpressionFactory::createComparison(EQ,expr,IntExpressionFactory::createConstant(1));
}


/// only valid for CONST expressions
/// use this call only in form : if (e.getType() == CONST) { int j = e.getValue() ; ...etc }
/// Exceptions will be thrown otherwise.
bool BoolExpression::getValue () const { 
  return  concrete->getExpr().getValue();
}
/// only valid for CONST expressions
/// use this call only in form : if (e.getType() == CONST) { int j = e.getValue() ; ...etc }
/// Exceptions will be thrown otherwise.
BoolExprType BoolExpression::getType () const { 
  return  concrete->getExpr().getType();
}

/// only valid for CONST expressions
/// use this call only in form : if (e.getType() == CONST) { int j = e.getValue() ; ...etc }
/// Exceptions will be thrown otherwise.
const labels_t & BoolExpression::getEnv () const { 
  return  concrete->getEnv();
}

/// only valid for CONST expressions
/// use this call only in form : if (e.getType() == CONST) { int j = e.getValue() ; ...etc }
/// Exceptions will be thrown otherwise.
const PBoolExpression & BoolExpression::getExpr () const { 
  return  concrete->getExpr();
}

BoolExpression BoolExpression::eval () const {
  return concrete->eval();
}

BoolExpression BoolExpression::operator& (const Assertion & a) const {
  return concrete->setAssertion(a);
}

bool BoolExpression::operator== (const BoolExpression & other) const {
  return concrete == other.concrete ;
}

void BoolExpression::print (std::ostream & os) const {
  concrete->print(os);
}

size_t BoolExpression::hash () const {
  return ddd::wang32_hash((size_t)concrete);
}

IntExpression BoolExpression::getFirstSubExpr () const {
  return concrete->getFirstSubExpr();
}


// binary comparisons
BoolExpression operator==(const IntExpression & l, const IntExpression & r) {
  return  BoolExpressionFactory::createComparison (EQ,l,r);
}
BoolExpression operator!=(const IntExpression & l, const IntExpression & r) {
  return  BoolExpressionFactory::createComparison (NEQ,l,r);
}
BoolExpression operator<=(const IntExpression & l, const IntExpression & r) {
  return  BoolExpressionFactory::createComparison (LEQ,l,r);
}
BoolExpression operator>=(const IntExpression & l, const IntExpression & r) {
  return  BoolExpressionFactory::createComparison (GEQ,l,r);
}
BoolExpression operator<(const IntExpression & l, const IntExpression & r) {
  return  BoolExpressionFactory::createComparison (LT,l,r);
}
BoolExpression operator>(const IntExpression & l, const IntExpression & r) {
  return  BoolExpressionFactory::createComparison (GT,l,r);
}

std::ostream & operator << (std::ostream & os, const BoolExpression & e) {
  e.print(os);
  return os;
}

// necessary administrative trivia
// refcounting
BoolExpression::BoolExpression (const _BoolExpression * concret): concrete(concret) {
  concrete->ref();
}

BoolExpression::BoolExpression (const BoolExpression & other) {
  if (this != &other) {
    concrete = other.concrete;
    concrete->ref();
  }
}

BoolExpression & BoolExpression::operator= (const BoolExpression & other) {
  if (this != &other) {
    // remove const qualifier for delete call
    BoolExpressionFactory::destroy((_BoolExpression *) concrete);
    concrete = other.concrete;
    concrete->ref();
  }
  return *this;
}

BoolExpression::~BoolExpression () {
  // remove const qualifier for delete call
  BoolExpressionFactory::destroy((_BoolExpression *) concrete);  
}



bool BoolExpression::isSupport (const Variable & v) const {
  return concrete->isSupport(v);
}

std::set<Variable> BoolExpression::getSupport() const {
  return concrete->getSupport();
}

} // namespace its


namespace d3 { namespace util {
  template<>
  struct hash<its::_IntExpression*> {
    size_t operator()(its::_IntExpression * g) const{
      return g->hash();
    }
  };

  template<>
  struct equal<its::_IntExpression*> {
    bool operator()(its::_IntExpression *g1,its::_IntExpression *g2) const{
      return (typeid(*g1) == typeid(*g2) && *g1 == *g2);
    }
  };
}} // namespaces d3::util




// end class IntExpression}
