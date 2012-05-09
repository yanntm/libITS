#include "PIntExpression.hh"
#include "PBoolExpression.hh"

#include <cmath>
#include <cassert>
#include "hashfunc.hh"
#include <typeinfo>


namespace its {

// for return in case of N/A
static NaryPParamType empty_params = NaryPParamType();

// unique storage class
class _PIntExpression {
  // count references
  mutable size_t refCount;
  // access to refcount for garbage purpose
  friend class PIntExpressionFactory;
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
  _PIntExpression (): refCount(0) {}
  // reclaim memory
  virtual ~_PIntExpression () { assert (refCount==0); };
  
  ///////// Interface functions
  // for hash storage
  virtual size_t hash () const = 0 ;
  virtual bool operator==(const _PIntExpression & e) const = 0;
  virtual _PIntExpression * clone () const = 0;

  // to avoid excessive typeid RTTI calls.
  virtual IntExprType getType() const =0;

  // Because friend is not transitive. Offer access to PIntExpression concrete to subclasses.
  static const _PIntExpression * getConcrete ( const PIntExpression & e) { return e.concrete ;}

  // pretty print
  virtual void print (std::ostream & os) const =0 ;

  // Evaluate an expression.
  virtual PIntExpression eval() const = 0;

  virtual PIntExpression setAssertion (const PAssertion & a) const {
    return a.getValue(this);
  }

  virtual bool isSupport (int varIndex, int id) const = 0;
  virtual void getSupport(bool * const mark) const = 0;

  virtual const NaryPParamType & getParams () const { 
    assert(false);
    return empty_params; }

  virtual int getVariable () const {
     throw "Do not call getVariable on non constant var expr or var int expressions.";
  }
  virtual int getValue () const {
    std::cerr << "called getValue on expression : " ;
    print(std::cerr);
    std::cerr << std::endl;
    throw "Do not call getValue on non constant int expressions.";
  }
 

  virtual PIntExpression reindexVariables (const PIntExpression::indexes_t & newindexes) const = 0;
  virtual PIntExpression getFirstSubExpr () const = 0;
};


// namespace std {
//   template<>
//   struct less<const PIntExpression &> {
//     bool operator()(const PIntExpression &g1,const PIntExpression &g2) const{
//       return g1 < g2;
//     }
//   };
// }


class VarExpr : public _PIntExpression {
  int varIndex;

public :
  VarExpr (int vvar) : varIndex(vvar){};
  IntExprType getType() const  { return VAR; }

  void print (std::ostream & os) const {
    os << "$" << varIndex;
  }

  PIntExpression eval () const {
    return this ;
  }

  bool operator==(const _PIntExpression & e) const {
    return varIndex == ((const VarExpr &)e).varIndex;
  }

  virtual size_t hash () const {
    return ddd::wang32_hash(varIndex) * 70019;
  }

  _PIntExpression * clone () const { return new VarExpr(*this); }

  bool isSupport (int v, int id) const {
    return varIndex == v;
  }
  
  void getSupport(bool * const  mark) const {
    mark[varIndex] = true;
  }

  PIntExpression getFirstSubExpr () const {
    return this;
  }

  int getVariable() const { return varIndex; }


  PIntExpression reindexVariables (const PIntExpression::indexes_t & newindexes) const {
    int newind = newindexes[varIndex];
    if (newind != varIndex) {
      return PIntExpressionFactory::createVariable(newind);
    }
    return this;
  }
};

class ConstExpr : public _PIntExpression {
  int val;

public :
  ConstExpr (int vval) : val(vval) {}
  IntExprType getType() const  { return CONST; }

  int getValue() const { return val; }

  PIntExpression eval () const {
    return this;
  }

  bool operator==(const _PIntExpression & e) const {
    return val == ((const ConstExpr &)e).val;
  }

  virtual size_t hash () const {
    return val * 70019;
  }

  _PIntExpression * clone () const { return new ConstExpr(*this); }

  void print (std::ostream & os) const {
    os << val;
  }

  bool isSupport (int,int) const {
    return false;
  }
  void getSupport(bool * const) const {
    return;
  }

  PIntExpression getFirstSubExpr () const {
    return this;
  }

  PIntExpression reindexVariables (const PIntExpression::indexes_t & ) const {
    return this ;
  }
};


class WrapBoolExpr : public _PIntExpression {
  PBoolExpression b;

public :
  WrapBoolExpr (const PBoolExpression & e) : b(e) {}
  IntExprType getType() const  { return WRAPBOOL; }

  PIntExpression eval () const {
    PBoolExpression bb = b.eval();
    if (bb.getType() == BOOLCONST) {
      if( bb.getValue() ) {
	return PIntExpressionFactory::createConstant(1);
      } else {
	return PIntExpressionFactory::createConstant(0);
      }
    }
    return PIntExpressionFactory::wrapBoolExpr(bb);
  }

  bool operator==(const _PIntExpression & e) const {
    return b == ((const WrapBoolExpr &)e).b;
  }

  virtual size_t hash () const {
    return b.hash() * 70019;
  }

  _PIntExpression * clone () const { return new WrapBoolExpr(*this); }

  void print (std::ostream & os) const {
    os << b;
  }

  bool isSupport (int v, int id) const {
    return b.isSupport(v,id);
  }
  void getSupport(bool * const mark) const {
    return b.getSupport(mark);
  }

  PIntExpression getFirstSubExpr () const {
    return b.getFirstSubExpr();
  }

  PIntExpression reindexVariables (const PIntExpression::indexes_t & index) const {
    return PIntExpressionFactory::wrapBoolExpr( b.reindexVariables(index));
  }

  PIntExpression setAssertion (const PAssertion & a) const {   
    PBoolExpression bb = b & a;
    PIntExpression wb = PIntExpressionFactory::wrapBoolExpr(bb).eval();
    return wb;
  }

};




class ArrayVarExpr : public _PIntExpression {
  int var;  
  PIntExpression index;
public :
  ArrayVarExpr (int vvar, const PIntExpression & index) : var(vvar), index(index) {};
  IntExprType getType() const  { return ARRAY; }

  void print (std::ostream & os) const {
    os << "$" << var << "[";
    index.print(os);
    os << "]";
  }

  PIntExpression eval () const {
    PIntExpression indeval = index.eval();
    if (indeval.equals(index)) {
      return this;
    } else {
      return PIntExpressionFactory::createArrayAccess (var, indeval);
    }
  }

  bool operator==(const _PIntExpression & e) const {
    return var == ((const ArrayVarExpr &)e).var && (index.equals( ((const ArrayVarExpr &)e).index));
  }

  virtual size_t hash () const {
    return (ddd::wang32_hash(var) * 70019) ^ index.hash();
  }

  _PIntExpression * clone () const { return new ArrayVarExpr(*this); }

  bool isSupport (int v,int id) const {
    return var == v || index.isSupport(v,id);
  }
  void getSupport(bool * const mark) const {
    mark[var] = true;
    index.getSupport(mark);
  }

  PIntExpression getFirstSubExpr () const {
    return index;
  }

  PIntExpression setAssertion (const PAssertion & a) const {
    PIntExpression tmp = index & a;
    return a.getValue(PIntExpressionFactory::createArrayAccess(var, tmp.eval()).eval());
  }

  PIntExpression reindexVariables (const PIntExpression::indexes_t & newindex) const {
    return PIntExpressionFactory::createArrayAccess( newindex[var], index.reindexVariables(newindex));
  }


};


class ArrayConstExpr : public _PIntExpression {
  int var;  
  PIntExpression index;
public :
  ArrayConstExpr (int vvar, const PIntExpression & index) : var(vvar), index(index) {};
  IntExprType getType() const  { return CONSTARRAY; }

  void print (std::ostream & os) const {
    os << "@" << var << "[";
    index.print(os);
    os << "]";
  }

  PIntExpression eval () const {
      return this;
  }

  bool operator==(const _PIntExpression & e) const {
    return var == ((const ArrayConstExpr &)e).var && (index.equals( ((const ArrayConstExpr &)e).index));
  }

  virtual size_t hash () const {
    return (ddd::wang32_hash(var) *  34019) ^ index.hash();
  }

  _PIntExpression * clone () const { return new ArrayConstExpr(*this); }

  bool isSupport (int v, int id) const {
    return var == v && id == index.getValue();
  }
  void getSupport(bool * const mark) const {
    mark[var] = true;
  }

  PIntExpression getFirstSubExpr () const {
    return this;
  }

  PIntExpression reindexVariables (const PIntExpression::indexes_t & newindex) const {
    return PIntExpressionFactory::createArrayAccess(newindex[var],  index);
  }

  int getVariable () const {
    return var;
  }

  int getValue () const {
    return index.getValue();
  }

  // fall back on default a.getValue(this)
//   PIntExpression setAssertion (const Assertion & a) const {   }


};




class NaryIntExpr : public _PIntExpression {
protected :
  NaryPParamType params ;
public :
  virtual const char * getOpString() const = 0;

  const NaryPParamType & getParams () const { return params; }
  
  NaryIntExpr (const NaryPParamType & pparams):params(pparams) {};

  // evaluate on two integers
  virtual int constEval (int i, int j) const = 0;
  // neutral element w.r.t. operation (e.g 1 for *, 0 for +)
  virtual int getNeutralElement () const = 0;


  PIntExpression eval () const {
    NaryPParamType p ;
    int constant = getNeutralElement();
    for (NaryPParamType::const_iterator it = params.begin(); it != params.end() ; ++it ) {
      PIntExpression e = it->eval();
      if (e.getType() == CONST) {
	constant = constEval(constant, e.getValue());
      } else {
	p.insert(e);
      }
    }
    if (constant != getNeutralElement() || p.empty())
      p.insert ( PIntExpressionFactory::createConstant(constant) );
    if (p.size() == 1) 
      return *p.begin();
    else 
      return PIntExpressionFactory::createNary(getType(),p);
  }


  void print (std::ostream & os) const {
    os << "( ";
    for (NaryPParamType::const_iterator it = params.begin() ;  ; ) {
      it->print(os);
      if (++it == params.end())
	break;
      else
	os << getOpString();
    }
    os << " )";
  }

  bool operator== (const _PIntExpression & e) const {
    const NaryIntExpr & other = (const NaryIntExpr &)e ;
    if (params.size() != other.params.size()) 
      return false;
    NaryPParamType::const_iterator it = params.begin();
    NaryPParamType::const_iterator jt = other.params.begin();
    for ( ; it != params.end()  ; ++it,++jt ) 
      if (! it->equals(*jt))
	return false;
    return true;
  }

  size_t hash () const {
    size_t res = getType();
    for (NaryPParamType::const_iterator it = params.begin() ; it != params.end()  ; ++it ) {
      res = res*(it->hash() +  76303);
    }
    return res;
  }

  PIntExpression setAssertion (const PAssertion & a) const {
    NaryPParamType res ;
    for (NaryPParamType::const_iterator it = params.begin() ; it != params.end()  ; ++it ) {
      res.insert( (*it) & a );
    }
    PIntExpression e = PIntExpressionFactory::createNary(getType(),res);    
    return a.getValue(e);
  }

  bool isSupport (int v, int id) const {
    for (NaryPParamType::const_iterator it = params.begin() ; it != params.end()  ; ++it ) {
      if (it->isSupport(v,id)) 
	return true;
    }
    return false;
  }
  
  void getSupport(bool * const mark) const {
    for (NaryPParamType::const_iterator it = params.begin() ; it != params.end() ; ++it) {
      it->getSupport(mark);
    }
  }

  PIntExpression reindexVariables (const PIntExpression::indexes_t & newindex) const {
    NaryPParamType res ;
    for (NaryPParamType::const_iterator it = params.begin() ; it != params.end()  ; ++it ) {
      res.insert( it->reindexVariables(newindex));
    }
    return PIntExpressionFactory::createNary(getType(),res);    
  }


  PIntExpression getFirstSubExpr () const {
    for (NaryPParamType::const_iterator it = params.begin() ; it != params.end()  ; ++it ) {
      PIntExpression tmp = it->getFirstSubExpr();
      if (! tmp.equals(*it)) {
	return tmp;
      }
    }
    return this;
  }


};

class PlusExpr : public NaryIntExpr {

public :
  PlusExpr (const NaryPParamType & pparams):NaryIntExpr(pparams) {};
  IntExprType getType() const  { return PLUS; }
  const char * getOpString() const { return " + ";}
  
  int constEval (int i, int j) const {
    return i+j;
  }
  int getNeutralElement () const {
    return 0;
  }

  _PIntExpression * clone () const { return new PlusExpr(*this); }
};

class MultExpr : public NaryIntExpr {

public :
  MultExpr (const NaryPParamType  & pparams):NaryIntExpr(pparams) {};
  IntExprType getType() const  { return MULT; }
  const char * getOpString() const { return " * ";}

  int constEval (int i, int j) const {
    return i*j;
  }
  int getNeutralElement () const {
    return 1;
  }
  _PIntExpression * clone () const { return new MultExpr(*this); }
};

class BinaryIntExpr : public _PIntExpression {
protected :
  PIntExpression left;
  PIntExpression right;
public :
  virtual const char * getOpString() const = 0;
  BinaryIntExpr (const PIntExpression & lleft, const PIntExpression & rright) : left (lleft),right(rright){};

  virtual int constEval (int i, int j) const = 0;

  PIntExpression eval () const {
    PIntExpression  l = left.eval();
    PIntExpression  r = right.eval();

    if (l.getType() == CONST && r.getType() == CONST ) {
      return  PIntExpressionFactory::createConstant( constEval( l.getValue(), r.getValue()) );
    } else {
      return  PIntExpressionFactory::createBinary( getType(), l, r );
    }
  }

  bool operator==(const _PIntExpression & e) const{
    const BinaryIntExpr & other = (const BinaryIntExpr &)e ;
    return other.left.equals(left) && other.right.equals(right);
  }
 
  size_t hash () const {
    size_t res = getType();
    res *= left.hash() *  76303 + right.hash() * 76147;
    return res;
  }
  void print (std::ostream & os) const {
    os << "( ";
    left.print(os);
    os << getOpString();
    right.print(os);
    os << " )";
  }

  PIntExpression setAssertion (const PAssertion & a) const {
    PIntExpression l = left & a;
    PIntExpression r = right & a;
    PIntExpression e = PIntExpressionFactory::createBinary(getType(),l,r);    
    return a.getValue(e);
  }

  bool isSupport (int v, int id) const {
    return left.isSupport(v,id) || right.isSupport(v,id);
  }
  
  void getSupport(bool *const mark) const {
    left.getSupport(mark);
    right.getSupport(mark);
  }

  PIntExpression getFirstSubExpr () const {

    {
      PIntExpression tmp = left.getFirstSubExpr();
      if (! tmp.equals(left)) {
	return tmp;
      }
    }
    {
      PIntExpression tmp = right.getFirstSubExpr();
      if (! tmp.equals(right)) {
	return tmp;
      }
    }
    return this;
  }


  PIntExpression reindexVariables (const PIntExpression::indexes_t & newindex) const {
    return PIntExpressionFactory::createBinary(getType(),left.reindexVariables(newindex), right.reindexVariables(newindex));    
  }


};

class MinusExpr : public BinaryIntExpr {

public :
  MinusExpr (const PIntExpression & left, const PIntExpression & right) : BinaryIntExpr(left,right) {};
  IntExprType getType() const  { return MINUS; }
  const char * getOpString() const { return " - ";}
  int constEval (int i, int j) const {
    return i-j;
  }
  _PIntExpression * clone () const { return new MinusExpr(*this); }

};

class DivExpr : public BinaryIntExpr {

public :
  DivExpr (const PIntExpression & left, const PIntExpression & right) : BinaryIntExpr(left,right) {};
  IntExprType getType() const  { return DIV; }
  const char * getOpString() const { return " / ";}

  int constEval (int i, int j) const {
    return i/j;
  }

  _PIntExpression * clone () const { return new DivExpr(*this); }

};

class ModExpr : public BinaryIntExpr {

public :
  ModExpr (const PIntExpression & left, const PIntExpression & right) : BinaryIntExpr(left,right) {};
  IntExprType getType() const  { return MOD; }
  const char * getOpString() const { return " % ";}

  int constEval (int i, int j) const {
    return i % j;
  }

  _PIntExpression * clone () const { return new ModExpr(*this); }

};

class PowExpr : public BinaryIntExpr {

public :
  PowExpr (const PIntExpression & left, const PIntExpression & right) : BinaryIntExpr(left,right) {};
  IntExprType getType() const  { return POW; }
  const char * getOpString() const { return " ** ";}

  int constEval (int i, int j) const {
    return int(pow(i,j));
  }
  _PIntExpression * clone () const { return new PowExpr(*this); }

};

class BitAndExpr : public BinaryIntExpr {

public :
  BitAndExpr (const PIntExpression & left, const PIntExpression & right) : BinaryIntExpr(left,right) {};
  IntExprType getType() const  { return BITAND; }
  const char * getOpString() const { return " & ";}
  int constEval (int i, int j) const {
    return i&j;
  }
  _PIntExpression * clone () const { return new BitAndExpr(*this); }

};

class BitOrExpr : public BinaryIntExpr {

public :
  BitOrExpr (const PIntExpression & left, const PIntExpression & right) : BinaryIntExpr(left,right) {};
  IntExprType getType() const  { return BITOR; }
  const char * getOpString() const { return " | ";}
  int constEval (int i, int j) const {
    return i|j;
  }
  _PIntExpression * clone () const { return new BitOrExpr(*this); }

};


class BitXorExpr : public BinaryIntExpr {

public :
  BitXorExpr (const PIntExpression & left, const PIntExpression & right) : BinaryIntExpr(left,right) {};
  IntExprType getType() const  { return BITXOR; }
  const char * getOpString() const { return " ^ ";}
  int constEval (int i, int j) const {
    return i^j;
  }
  _PIntExpression * clone () const { return new BitXorExpr(*this); }

};


class BitLshiftExpr : public BinaryIntExpr {

public :
  BitLshiftExpr (const PIntExpression & left, const PIntExpression & right) : BinaryIntExpr(left,right) {};
  IntExprType getType() const  { return LSHIFT; }
  const char * getOpString() const { return " << ";}
  int constEval (int i, int j) const {
    return i << j;
  }
  _PIntExpression * clone () const { return new BitLshiftExpr(*this); }

};


class BitRshiftExpr : public BinaryIntExpr {

public :
  BitRshiftExpr (const PIntExpression & left, const PIntExpression & right) : BinaryIntExpr(left,right) {};
  IntExprType getType() const  { return BITOR; }
  const char * getOpString() const { return " >> ";}
  int constEval (int i, int j) const {
    return i>>j;
  }
  _PIntExpression * clone () const { return new BitRshiftExpr(*this); }

};


/********************************************************/
/***********  Assertion *********************************/

PAssertion::PAssertion (const PIntExpression & var, const PIntExpression & val) : mapping(var,val) {};

PIntExpression PAssertion::getValue (const PIntExpression & v) const {
  if (v.equals(mapping.first)) 
    return mapping.second;
  else 
    return v;
}

size_t PAssertion::hash() const {
  return mapping.first.hash() ^ mapping.second.hash();
}

bool PAssertion::operator== (const PAssertion & other) const {
  return mapping.first.equals(other.mapping.first) && mapping.second.equals(other.mapping.second);
}
  
bool PAssertion::operator< (const PAssertion & other) const {
  return mapping.first.equals(other.mapping.first) ? mapping.second.less(other.mapping.second) : mapping.first.less(other.mapping.first);
}

PAssertion PAssertion::operator & (const PAssertion & other) const {
  return PIntExpressionFactory::createPAssertion(mapping.first, (mapping.second & other).eval());
}

/// To determine whether a given variable is mentioned in an expression.
bool PAssertion::isSupport (int v, int id) const {
  return mapping.first.isSupport(v,id) || mapping.second.isSupport(v,id);
}

void PAssertion::print (std::ostream & os) const {
  os << mapping.first << ":=" << mapping.second ;
}



/*******************************************************/
/******* Factory ***************************************/
// namespace PIntExpressionFactory {

UniqueTable<_PIntExpression>  PIntExpressionFactory::unique = UniqueTable<_PIntExpression>();


PIntExpression PIntExpressionFactory::createNary (IntExprType type, const NaryPParamType & params) {
  NaryPParamType pp;

  for (NaryPParamType::const_iterator it = params.begin() ; it != params.end() ; ++it ){
    if ( it->getType() == type ) {
      for (NaryPParamType::const_iterator jt = it->getParams().begin() ; jt != it->getParams().end() ; ++jt ) {
	pp.insert(*jt);
      }
    } else {
      pp.insert(*it);
    }
  }

  switch (type) {
  case PLUS :
    return unique(PlusExpr (params));      
  case MULT :
    return unique(MultExpr (params));      
  default :
    throw "Operator " + to_string(type)  + " is not N-ary";
  }
}

PIntExpression PIntExpressionFactory::createBinary (IntExprType type, const PIntExpression & l, const PIntExpression & r) {
  switch (type) {
  case MINUS :
    return unique(MinusExpr (l,r));      
  case DIV :
    return unique(DivExpr (l,r));      
  case MOD :
    return unique(ModExpr (l,r));      
  case POW :
    return unique(PowExpr (l,r));
  case BITAND :
    return unique(BitAndExpr (l,r));
  case BITOR :
    return unique(BitOrExpr (l,r));
  case BITXOR :
    return unique(BitXorExpr (l,r));
  case LSHIFT :
    return unique(BitLshiftExpr (l,r));
  case RSHIFT :
    return unique(BitRshiftExpr (l,r));
  case PLUS :
  case MULT :
    {
      NaryPParamType params;
      params.insert(l);
      params.insert(r);
      return createNary (type,params);
    }
  default :
    throw "Operator " + to_string(type)  + " is not binary";
  }
}

PIntExpression PIntExpressionFactory::createConstant (int v) {
  return unique (ConstExpr(v));
}

PIntExpression PIntExpressionFactory::createVariable (int  v) {
  return unique (VarExpr(v));
}

PIntExpression PIntExpressionFactory::createArrayAccess (int v, const PIntExpression & index) {
  if (index.getType() != CONST) 
    return unique (ArrayVarExpr(v,index));
  else
    return unique (ArrayConstExpr(v,index));
}

PIntExpression PIntExpressionFactory::wrapBoolExpr (const PBoolExpression &b) {
  return unique (WrapBoolExpr(b));
}


PAssertion PIntExpressionFactory::createPAssertion (const PIntExpression & v,const PIntExpression & e) {
  return PAssertion(v,e);
}

const _PIntExpression * PIntExpressionFactory::createUnique(const _PIntExpression &e) {
  return unique(e);
}

void PIntExpressionFactory::destroy (_PIntExpression * e) {
  if (  e->deref() == 0 ){
    UniqueTable<_PIntExpression>::Table::iterator ci = unique.table.find(e);
    assert (ci != unique.table.end());
    unique.table.erase(ci);
    delete e;
  }
}

void PIntExpressionFactory::printStats (std::ostream &os) {
  os << "Integer expression entries :" << unique.size() << std::endl;
}

// } end namespace PIntExpressionFactory


// namespace PIntExpression {
// friend operator
PIntExpression operator+(const PIntExpression & l,const PIntExpression & r) {  
  return PIntExpressionFactory::createBinary(PLUS,l,r);
} 

PIntExpression operator*(const PIntExpression & l,const PIntExpression & r) {  
  return PIntExpressionFactory::createBinary(MULT, l, r);
} 



// necessary administrative trivia
// refcounting
PIntExpression::PIntExpression (const _PIntExpression * concret): concrete(concret) {
  concrete->ref();
}

PIntExpression::PIntExpression (const PIntExpression & other) {
  if (this != &other) {
    concrete = other.concrete;
    concrete->ref();
  }
}


PIntExpression::PIntExpression (int cst) {
  concrete = PIntExpressionFactory::createUnique(ConstExpr(cst));
  concrete->ref();
}


PIntExpression::~PIntExpression () {
  // remove const qualifier for delete call
  PIntExpressionFactory::destroy((_PIntExpression *) concrete);  
}

PIntExpression & PIntExpression::operator= (const PIntExpression & other) {
  if (this != &other) {
    // remove const qualifier for delete call
    PIntExpressionFactory::destroy((_PIntExpression *) concrete);
    concrete = other.concrete;
    concrete->ref();
  }
  return *this;
}

bool PIntExpression::equals (const PIntExpression & other) const {
  return concrete == other.concrete ;
}

bool PIntExpression::less (const PIntExpression & other) const {
  return concrete < other.concrete;
}

PIntExpression PIntExpression::getFirstSubExpr () const {
  return concrete->getFirstSubExpr();
}

PIntExpression PIntExpression::reindexVariables (const indexes_t & newindexes) const {
  return concrete->reindexVariables (newindexes);
}

const NaryPParamType & PIntExpression::getParams() const {
  return concrete->getParams();
}



void PIntExpression::print (std::ostream & os) const {
  concrete->print(os);
}

PIntExpression PIntExpression::eval () const {
  return concrete->eval();
}

/// only valid for CONST expressions
/// use this call only in form : if (e.getType() == CONST) { int j = e.getValue() ; ...etc }
/// Exceptions will be thrown otherwise.
int PIntExpression::getValue () const {
  return concrete->getValue();    
}

int PIntExpression::getVariable () const {
  return concrete->getVariable();    
}



bool PIntExpression::isSupport(int var, int id) const {
  return concrete->isSupport(var, id);
}

void PIntExpression::getSupport(bool * const mark) const {
  concrete->getSupport(mark);
}

PIntExpression PIntExpression::operator& (const PAssertion &a) const {
  return concrete->setAssertion(a);
}

IntExprType PIntExpression::getType() const {
  return concrete->getType();
}
// binary
PIntExpression operator-(const PIntExpression & l,const PIntExpression & r)  {
  return PIntExpressionFactory::createBinary(MINUS,l,r);
}
// unary -
PIntExpression operator-(const PIntExpression & l)  {
  return PIntExpressionFactory::createBinary(MINUS,  PIntExpressionFactory::createConstant(0),l);
}


PIntExpression operator/(const PIntExpression & l,const PIntExpression & r) {
  return PIntExpressionFactory::createBinary(DIV,l,r);
}
PIntExpression operator%(const PIntExpression & l,const PIntExpression & r) {
  return PIntExpressionFactory::createBinary(MOD,l,r);
}
PIntExpression operator^(const PIntExpression & l,const PIntExpression & r) {
  return PIntExpressionFactory::createBinary(POW,l,r);
}

size_t PIntExpression::hash () const { 
  return ddd::knuth32_hash(reinterpret_cast<const size_t>(concrete)); 
}

std::ostream & operator << (std::ostream & os, const its::PIntExpression & e) {
  e.print(os);
  return os;
}




} // namespace its


namespace d3 { namespace util {
  template<>
  struct hash<its::_PIntExpression*> {
    size_t operator()(its::_PIntExpression * g) const{
      return g->hash();
    }
  };

  template<>
  struct equal<its::_PIntExpression*> {
    bool operator()(its::_PIntExpression *g1,its::_PIntExpression *g2) const{
      return (typeid(*g1) == typeid(*g2) && *g1 == *g2);
    }
  };
}} // namespaces d3::util




// end class PIntExpression}