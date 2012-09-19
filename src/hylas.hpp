#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <climits>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <csetjmp>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <stdarg.h>

/*#include "llvm/DerivedTypes.h"
 * #include "llvm/ExecutionEngine/ExecutionEngine.h"
 * #include "llvm/ExecutionEngine/JIT.h"
 * #include "llvm/LLVMContext.h"
 * #include "llvm/Module.h"
 * #include "llvm/PassManager.h"
 * #include "llvm/Analysis/Verifier.h"
 * #include "llvm/Analysis/Passes.h"
 * #include "llvm/Target/TargetData.h"
 * #include "llvm/Transforms/Scalar.h"
 * #include "llvm/Support/IRBuilder.h"
 * #include "llvm/Support/TargetSelect.h"
 * #include "llvm/Support/raw_ostream.h"
 * #include "llvm/Support/SourceMgr.h"
 * #include "llvm/ADT/Twine.h"
 * #include "llvm/Assembly/Parser.h"*/


namespace Hylas
{
  using namespace std;
  //using namespace llvm;
  
  /* Module* Program;
   *  LLVMContext& Context = getGlobalContext();
   *  IRBuilder<> Builder(Context);
   *  FunctionPassManager FPM(Program);
   *  ExecutionEngine* Engine;*/
  
  #define List  false
  #define Atom  true
  
  struct Form
  {
    bool        Tag;
    string      Value;
    Form*       Car;
    Form*       Cdr;
    long line;
    int column;
    ~Form()
    {
      delete Car; delete Cdr;
    }
  };
  
  #define val(x)        (x->Value)
  #define tag(x)        (x->Tag)
  #define isatom(x)     (tag(x) == Atom)
  #define islist(x)     (tag(x) == List)
  #define car(x)        (x->Car)
  #define cdr(x)        (x->Cdr)
  #define cadr(x)       (car(cdr(x)))
  #define cddr(x)       (cdr(cdr(x)))
  
  typedef void (*entryfnptr)();
  
  void Unwind();
  Form* cons(Form* first, Form* second);
  unsigned long length(Form* in);
  Form* nth(Form* in, long location);
  inline Form* makeForm(string in, bool tag);
  string next_token(FILE *in);
  Form* read_tail(FILE *in);
  Form* read(FILE* in);
  string print(Form* in);
  unsigned char analyze(string in);
  void atomize(string atom);
  string* lookup(string in);
  string type(string input);
  string code(string input);
  string emitCode(Form* form);
  
  ostream& operator<<(ostream& out, Form* in)
  { out << print(in); return out; }
  
  string operator+(string out, Form* in)
  { return out + print(in); }
  
  string operator+(Form* in, string out)
  { return print(in) + out; }
  
  struct Compiler
  {
        bool allow_RedefineMacros;
        bool allow_RedefineFunctions;
        bool output;
  };
  
  Compiler master;
  
  #define plain         false
  #define HTML          true
 
  #include "utils.hpp"
  #include "errors.hpp"
  #include "reader.hpp"
  
  struct Variable
  {
    string sym;
    bool constant;
    bool argument;
  };
  
  typedef map<string,Variable> Scope;
  typedef vector<Scope> ST;
  ST SymbolTable;
  #define ScopeDepth SymbolTable.size()-1
  
  #define error_unbound(x)        \
  printf("ERROR:Symbol '%s' is unbound.",print(x).c_str()); \
  Unwind();
  
  string* lookup(string in)
  {
    for(long i = ScopeDepth; i != -1; i--)
    {
      map<string,Variable>::iterator seeker = SymbolTable[i].find(in);
      if(seeker != SymbolTable[i].end())
        return &seeker->second.sym;
    }
    return NULL;
  }
  
  unsigned long tmp_version = -1;
  unsigned long res_version = -1;
  unsigned long label_version = -1;
  unsigned long string_version = -1;
  unsigned long array_version = -1;
  
  string cutlast(string in)
  {
    return string(in,0,in.length()-1);
  }
  
  /*
   * TEMPORARY REGISTERS
   */
  
  inline string get_unique_tmp()
  {
    return "%tmp.version" + to_string(++tmp_version);
  }
  
  inline string get_tmp(long long int v)
  {
    return "%tmp.version" + to_string(v);
  }
  
  inline string get_current_tmp(){ return get_tmp(tmp_version);}
  
  /*
   * RESULT REGISTERS
   */
  
  string get_unique_res(string type)
  {
    string vnum = to_string(++res_version);
    SymbolTable[ScopeDepth]["%res.version" + vnum].sym = type;
    return "%res.version" + vnum;
  }
  
  inline string get_res(long v)
  {
    return "%res.version" + to_string(v);
  }
  
  string res_type(string name)
  {
    string* tmp = lookup(name);
    if(tmp == NULL)
    { error("Can't find the register '",name,"'."); }
    return *tmp;
  }
  
  inline string get_current_res(){ return get_res(res_version);}
  
  /*
   * LABELS
   */
  
  string get_unique_label()
  {
    return "%label.version" + to_string(label_version++);
  }
  
  string get_label(long long int v)
  {
    return "%label.version" + to_string(v);
  }
  
  inline string get_current_label(){ return get_label(label_version);}
  
  #define allocate(address,type)                \
  (string)address + " = alloca " + type + "\n"
  #define store(type,value,address)             \
  (string)"store " + type + " " + value + ", " + type + "* " + address + "\n"
  #define load(to,type,source)                  \
  (string)to + " = load " + type + "* " + source
  
  #define latest_type()   res_type(get_current_res())
  
  void dump_scope(unsigned long s)
  {
    for(map<string,Variable>::iterator i = SymbolTable[s].begin();
        i != SymbolTable[s].end(); i++)
    {
      printf("\n %s : %s",i->first.c_str(),i->second.sym.c_str());
    }
  }
  
  vector<string> CodeStack;
  
  inline void push(string in)
  {
    CodeStack.push_back(in);
  }

  #include "types.hpp"
  #include "fndef.hpp"
  #include "core.hpp"
  #include "tests.hpp"
  #include "macros.hpp"
  
  string emitCode(Form* form)
  {
    string out;
    if(form == NULL)
      error("Can't emit code for the null form ().");
    else if(isatom(form))
    {
      switch(analyze(val(form)))
      {
        case BooleanTrue:
        {
          out = allocate(get_unique_tmp(),"i1");
          out += store("i1","1",get_current_tmp());
          out += load(get_unique_res("i1"),"i1",get_current_tmp());
          break;
        }
        case BooleanFalse:
        {
          out = allocate(get_unique_tmp(),"i1");
          out += store("i1","0",get_current_tmp());
          out += load(get_unique_res("i1"),"i1",get_current_tmp());
          break;
        }
        case Integer:
        {
          out = allocate(get_unique_tmp(),"i64");
          out += store("i64",val(form),get_current_tmp());
          out += load(get_unique_res("i64"),"i64",get_current_tmp());
          break;
        }
        case Real:
        {
          out = allocate(get_unique_tmp(),"double");
          out += store("double",val(form),get_current_tmp());
          out += load(get_unique_res("double"),"double",get_current_tmp());
          break;
        }
        case Symbol:
        {
          /*map<string,string>::iterator seeker;
          string sym = val(form);
          string tmp;
          string type;
          for(long i = ScopeDepth; i != -1; i--)
          {
            seeker = SymbolTable[i].find(sym);
            if(seeker != SymbolTable[i].end())
            {
              type = seeker->second;
              tmp = sym + to_string<long>(i);
              out = load(get_unique_res(type),type,"%"+tmp);
            }
          }
          string* tmp = lookup(val(form));
          if(tmp != NULL)
            out = load(get_unique_res(val(form)),val(form),"%"+val(form));
          else
            error_unbound(val(form));
          break;*/
          string sym = val(form);
          string* tmp = lookup(sym);
          if(tmp == NULL)
          {
            error_unbound(form);
          }
          else
            out = load(get_unique_res(*tmp),*tmp,"%"+sym+to_string(ScopeDepth));
          break;
        }
        case Character:
        {
          string c = string(val(form),1,val(form).length()-2);
          string address = "@str" + to_string<unsigned long>(++string_version);
          out = allocate(get_unique_tmp(),"i8");
          push(address + " = global [2 x i8] c\"" + c + "\0\0\"");
          out += get_unique_tmp() + " = load i8* getelementptr inbounds ([2 x i8]* " + address + ", i32 0, i64 0)";
          out += store("i8",get_current_tmp(),get_tmp(tmp_version-1));
          out += load(get_unique_res("i8"),"i8",get_current_tmp());
          break;
        }
        case String:
        {
          //Remember strings come with their double quotes at the end
          string str = cutlast(string(val(form),1));
          long length = str.length()-1;
          string type = "[" + to_string<long>(length) + " x i8]";
          push("@str" + to_string<unsigned long>(++string_version) + " = global " + type + " c\"" + str + "\\0A\\00\"");
          out = get_unique_res(type) + " = getelementptr " + type + "* @str" + to_string<unsigned long>(string_version) + ", i64 0, i64 0";
          break;
        }
      }
    }
    else
    {
      if(islist(car(form)))
        error("Lists can't be used as function names in calls. Until I implement lambda.",at(form));
      string func = val(car(form));
      map<string,hFuncPtr>::iterator seeker = Core.find(func);
      if(seeker != Core.end())
        out = seeker->second(form);
      else
        out = callFunction(func,cdr(form));
    }
    return out+"\n";
  }
  
  string Compile(Form* form)
  {
    string out;
    string tmp;
    if(form == NULL)
      error("Can't emit code for the null form.");
    else if(isatom(form))
    {
      out = emitCode(form);
    }
    else
    {
      if(islist(car(form)))
        error("Lists can't be used as function names in calls. Until I implement lambda.",at(form));
      string func = val(car(form));
      map<string,hFuncPtr>::iterator seeker = TopLevel.find(func);
      if(seeker != TopLevel.end())
      {
        push(seeker->second(form));
        out += emitCode(readString("true"));
      }
      else
      {
        seeker = Core.find(func);
        if(seeker != Core.end())
          out = seeker->second(form);
        else
          out = callFunction(func,cdr(form));
      }
    }
    for(unsigned long i = 0; i < CodeStack.size(); i++)
      tmp += CodeStack[i] + "\n";
    out = "define " + latest_type() + " @entry(){\n" + out + "\nret " + latest_type() + " " + get_current_res() + "\n}";
    out = tmp + out;
    CodeStack.clear();
    return out;
  }
  
  void init_optimizer()
  {
    /*FPM.add(createBasicAliasAnalysisPass());
     *    FPM.add(createInstructionCombiningPass());
     *    FPM.add(createReassociatePass());
     *    FPM.add(createGVNPass());
     *    FPM.add(createCFGSimplificationPass());
     *    FPM.add(createPromoteMemoryToRegisterPass());
     *    FPM.doInitialization();*/
  }
  
  void init()
  {
    //InitializeNativeTarget();
    //Program = new Module("Hylas Lisp",Context);
    master.allow_RedefineMacros = true;
    master.allow_RedefineFunctions = false;
    master.output = plain;
    init_stdlib();
    init_types();
    init_optimizer();
    //Engine =  EngineBuilder(Program).create();
  }
  
  /*void compileIR(string in)
   *  {
   *    SMDiagnostic errors;
   *    ParseAssemblyString(in.c_str(),Program,errors,Context);
   *    if(!errors.getMessage().empty())
   *      printf("\n%s",errors.getMessage().c_str());
   *    if(verifyModule(*Program))
   *    {
   *      printf("The IR verifier found an unknown error.");
   *      Unwind();
   }
   }*/  
}