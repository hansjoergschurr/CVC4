/*********************                                                        */
/*! \file command.h
 ** \verbatim
 ** Original author: Morgan Deters
 ** Major contributors: none
 ** Minor contributors (to current version): Kshitij Bansal, Christopher L. Conway, Dejan Jovanovic, Francois Bobot, Andrew Reynolds
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2014  New York University and The University of Iowa
 ** See the file COPYING in the top-level source directory for licensing
 ** information.\endverbatim
 **
 ** \brief Implementation of the command pattern on SmtEngines.
 **
 ** Implementation of the command pattern on SmtEngines.  Command
 ** objects are generated by the parser (typically) to implement the
 ** commands in parsed input (see Parser::parseNextCommand()), or by
 ** client code.
 **/

#include "cvc4_public.h"

#ifndef __CVC4__COMMAND_H
#define __CVC4__COMMAND_H

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

#include "expr/expr.h"
#include "expr/type.h"
#include "expr/variable_type_map.h"
#include "util/result.h"
#include "util/sexpr.h"
#include "util/datatype.h"
#include "util/proof.h"
#include "util/unsat_core.h"

namespace CVC4 {

class SmtEngine;
class Command;
class CommandStatus;
class Model;

std::ostream& operator<<(std::ostream&, const Command&) throw() CVC4_PUBLIC;
std::ostream& operator<<(std::ostream&, const Command*) throw() CVC4_PUBLIC;
std::ostream& operator<<(std::ostream&, const CommandStatus&) throw() CVC4_PUBLIC;
std::ostream& operator<<(std::ostream&, const CommandStatus*) throw() CVC4_PUBLIC;

/** The status an SMT benchmark can have */
enum BenchmarkStatus {
  /** Benchmark is satisfiable */
  SMT_SATISFIABLE,
  /** Benchmark is unsatisfiable */
  SMT_UNSATISFIABLE,
  /** The status of the benchmark is unknown */
  SMT_UNKNOWN
};/* enum BenchmarkStatus */

std::ostream& operator<<(std::ostream& out,
                         BenchmarkStatus status) throw() CVC4_PUBLIC;

/**
 * IOStream manipulator to print success messages or not.
 *
 *   out << Command::printsuccess(false) << CommandSuccess();
 *
 * prints nothing, but
 *
 *   out << Command::printsuccess(true) << CommandSuccess();
 *
 * prints a success message (in a manner appropriate for the current
 * output language).
 */
class CVC4_PUBLIC CommandPrintSuccess {
  /**
   * The allocated index in ios_base for our depth setting.
   */
  static const int s_iosIndex;

  /**
   * The default setting, for ostreams that haven't yet had a
   * setdepth() applied to them.
   */
  static const int s_defaultPrintSuccess = false;

  /**
   * When this manipulator is used, the setting is stored here.
   */
  bool d_printSuccess;

public:
  /**
   * Construct a CommandPrintSuccess with the given setting.
   */
  CommandPrintSuccess(bool printSuccess) throw() : d_printSuccess(printSuccess) {}

  inline void applyPrintSuccess(std::ostream& out) throw() {
    out.iword(s_iosIndex) = d_printSuccess;
  }

  static inline bool getPrintSuccess(std::ostream& out) throw() {
    return out.iword(s_iosIndex);
  }

  static inline void setPrintSuccess(std::ostream& out, bool printSuccess) throw() {
    out.iword(s_iosIndex) = printSuccess;
  }

  /**
   * Set the print-success state on the output stream for the current
   * stack scope.  This makes sure the old state is reset on the
   * stream after normal OR exceptional exit from the scope, using the
   * RAII C++ idiom.
   */
  class Scope {
    std::ostream& d_out;
    bool d_oldPrintSuccess;

  public:

    inline Scope(std::ostream& out, bool printSuccess) throw() :
      d_out(out),
      d_oldPrintSuccess(CommandPrintSuccess::getPrintSuccess(out)) {
      CommandPrintSuccess::setPrintSuccess(out, printSuccess);
    }

    inline ~Scope() throw() {
      CommandPrintSuccess::setPrintSuccess(d_out, d_oldPrintSuccess);
    }

  };/* class CommandPrintSuccess::Scope */

};/* class CommandPrintSuccess */

/**
 * Sets the default print-success setting when pretty-printing an Expr
 * to an ostream.  Use like this:
 *
 *   // let out be an ostream, e an Expr
 *   out << Expr::setdepth(n) << e << endl;
 *
 * The depth stays permanently (until set again) with the stream.
 */
inline std::ostream& operator<<(std::ostream& out, CommandPrintSuccess cps) throw() CVC4_PUBLIC;
inline std::ostream& operator<<(std::ostream& out, CommandPrintSuccess cps) throw() {
  cps.applyPrintSuccess(out);
  return out;
}

class CVC4_PUBLIC CommandStatus {
protected:
  // shouldn't construct a CommandStatus (use a derived class)
  CommandStatus() throw() {}
public:
  virtual ~CommandStatus() throw() {}
  void toStream(std::ostream& out,
                OutputLanguage language = language::output::LANG_AUTO) const throw();
  virtual CommandStatus& clone() const = 0;
};/* class CommandStatus */

class CVC4_PUBLIC CommandSuccess : public CommandStatus {
  static const CommandSuccess* s_instance;
public:
  static const CommandSuccess* instance() throw() { return s_instance; }
  CommandStatus& clone() const { return const_cast<CommandSuccess&>(*this); }
};/* class CommandSuccess */

class CVC4_PUBLIC CommandUnsupported : public CommandStatus {
public:
  CommandStatus& clone() const { return *new CommandUnsupported(*this); }
};/* class CommandSuccess */

class CVC4_PUBLIC CommandFailure : public CommandStatus {
  std::string d_message;
public:
  CommandFailure(std::string message) throw() : d_message(message) {}
  CommandFailure& clone() const { return *new CommandFailure(*this); }
  ~CommandFailure() throw() {}
  std::string getMessage() const throw() { return d_message; }
};/* class CommandFailure */

class CVC4_PUBLIC Command {
protected:
  /**
   * This field contains a command status if the command has been
   * invoked, or NULL if it has not.  This field is either a
   * dynamically-allocated pointer, or it's a pointer to the singleton
   * CommandSuccess instance.  Doing so is somewhat asymmetric, but
   * it avoids the need to dynamically allocate memory in the common
   * case of a successful command.
   */
  const CommandStatus* d_commandStatus;

  /**
   * True if this command is "muted"---i.e., don't print "success" on
   * successful execution.
   */
  bool d_muted;

public:
  typedef CommandPrintSuccess printsuccess;

  Command() throw();
  Command(const Command& cmd);

  virtual ~Command() throw();

  virtual void invoke(SmtEngine* smtEngine) throw() = 0;
  virtual void invoke(SmtEngine* smtEngine, std::ostream& out) throw();

  virtual void toStream(std::ostream& out, int toDepth = -1, bool types = false, size_t dag = 1,
                        OutputLanguage language = language::output::LANG_AUTO) const throw();

  std::string toString() const throw();

  virtual std::string getCommandName() const throw() = 0;

  /**
   * If false, instruct this Command not to print a success message.
   */
  void setMuted(bool muted) throw() { d_muted = muted; }

  /**
   * Determine whether this Command will print a success message.
   */
  bool isMuted() throw() { return d_muted; }

  /**
   * Either the command hasn't run yet, or it completed successfully
   * (CommandSuccess, not CommandUnsupported or CommandFailure).
   */
  bool ok() const throw();

  /**
   * The command completed in a failure state (CommandFailure, not
   * CommandSuccess or CommandUnsupported).
   */
  bool fail() const throw();

  /** Get the command status (it's NULL if we haven't run yet). */
  const CommandStatus* getCommandStatus() const throw() { return d_commandStatus; }

  virtual void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();

  /**
   * Maps this Command into one for a different ExprManager, using
   * variableMap for the translation and extending it with any new
   * mappings.
   */
  virtual Command* exportTo(ExprManager* exprManager,
                            ExprManagerMapCollection& variableMap) = 0;

  /**
   * Clone this Command (make a shallow copy).
   */
  virtual Command* clone() const = 0;

protected:
  class ExportTransformer {
    ExprManager* d_exprManager;
    ExprManagerMapCollection& d_variableMap;
  public:
    ExportTransformer(ExprManager* exprManager, ExprManagerMapCollection& variableMap) :
      d_exprManager(exprManager),
      d_variableMap(variableMap) {
    }
    Expr operator()(Expr e) {
      return e.exportTo(d_exprManager, d_variableMap);
    }
    Type operator()(Type t) {
      return t.exportTo(d_exprManager, d_variableMap);
    }
  };/* class Command::ExportTransformer */
};/* class Command */

/**
 * EmptyCommands are the residue of a command after the parser handles
 * them (and there's nothing left to do).
 */
class CVC4_PUBLIC EmptyCommand : public Command {
protected:
  std::string d_name;
public:
  EmptyCommand(std::string name = "") throw();
  ~EmptyCommand() throw() {}
  std::string getName() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class EmptyCommand */

class CVC4_PUBLIC EchoCommand : public Command {
protected:
  std::string d_output;
public:
  EchoCommand(std::string output = "") throw();
  ~EchoCommand() throw() {}
  std::string getOutput() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  void invoke(SmtEngine* smtEngine, std::ostream& out) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class EchoCommand */

class CVC4_PUBLIC AssertCommand : public Command {
protected:
  Expr d_expr;
  bool d_inUnsatCore;
public:
  AssertCommand(const Expr& e, bool inUnsatCore = true) throw();
  ~AssertCommand() throw() {}
  Expr getExpr() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class AssertCommand */

class CVC4_PUBLIC PushCommand : public Command {
public:
  ~PushCommand() throw() {}
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class PushCommand */

class CVC4_PUBLIC PopCommand : public Command {
public:
  ~PopCommand() throw() {}
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class PopCommand */

class CVC4_PUBLIC DeclarationDefinitionCommand : public Command {
protected:
  std::string d_symbol;
public:
  DeclarationDefinitionCommand(const std::string& id) throw();
  ~DeclarationDefinitionCommand() throw() {}
  virtual void invoke(SmtEngine* smtEngine) throw() = 0;
  std::string getSymbol() const throw();
};/* class DeclarationDefinitionCommand */

class CVC4_PUBLIC DeclareFunctionCommand : public DeclarationDefinitionCommand {
protected:
  Expr d_func;
  Type d_type;
  bool d_printInModel;
  bool d_printInModelSetByUser;
public:
  DeclareFunctionCommand(const std::string& id, Expr func, Type type) throw();
  ~DeclareFunctionCommand() throw() {}
  Expr getFunction() const throw();
  Type getType() const throw();
  bool getPrintInModel() const throw();
  bool getPrintInModelSetByUser() const throw();
  void setPrintInModel( bool p );
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class DeclareFunctionCommand */

class CVC4_PUBLIC DeclareTypeCommand : public DeclarationDefinitionCommand {
protected:
  size_t d_arity;
  Type d_type;
public:
  DeclareTypeCommand(const std::string& id, size_t arity, Type t) throw();
  ~DeclareTypeCommand() throw() {}
  size_t getArity() const throw();
  Type getType() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class DeclareTypeCommand */

class CVC4_PUBLIC DefineTypeCommand : public DeclarationDefinitionCommand {
protected:
  std::vector<Type> d_params;
  Type d_type;
public:
  DefineTypeCommand(const std::string& id, Type t) throw();
  DefineTypeCommand(const std::string& id, const std::vector<Type>& params, Type t) throw();
  ~DefineTypeCommand() throw() {}
  const std::vector<Type>& getParameters() const throw();
  Type getType() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class DefineTypeCommand */

class CVC4_PUBLIC DefineFunctionCommand : public DeclarationDefinitionCommand {
protected:
  Expr d_func;
  std::vector<Expr> d_formals;
  Expr d_formula;
public:
  DefineFunctionCommand(const std::string& id, Expr func, Expr formula) throw();
  DefineFunctionCommand(const std::string& id, Expr func,
                        const std::vector<Expr>& formals, Expr formula) throw();
  ~DefineFunctionCommand() throw() {}
  Expr getFunction() const throw();
  const std::vector<Expr>& getFormals() const throw();
  Expr getFormula() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class DefineFunctionCommand */

/**
 * This differs from DefineFunctionCommand only in that it instructs
 * the SmtEngine to "remember" this function for later retrieval with
 * getAssignment().  Used for :named attributes in SMT-LIBv2.
 */
class CVC4_PUBLIC DefineNamedFunctionCommand : public DefineFunctionCommand {
public:
  DefineNamedFunctionCommand(const std::string& id, Expr func,
                             const std::vector<Expr>& formals, Expr formula) throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
};/* class DefineNamedFunctionCommand */

/**
 * The command when an attribute is set by a user.  In SMT-LIBv2 this is done
 *  via the syntax (! expr :attr)
 */
class CVC4_PUBLIC SetUserAttributeCommand : public Command {
protected:
  std::string d_attr;
  Expr d_expr;
  std::vector<Expr> d_expr_values;
  std::string d_str_value;
public:
  SetUserAttributeCommand( const std::string& attr, Expr expr ) throw();
  SetUserAttributeCommand( const std::string& attr, Expr expr, std::vector<Expr>& values ) throw();
  SetUserAttributeCommand( const std::string& attr, Expr expr, const std::string& value ) throw();
  ~SetUserAttributeCommand() throw() {}
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class SetUserAttributeCommand */


class CVC4_PUBLIC CheckSatCommand : public Command {
protected:
  Expr d_expr;
  Result d_result;
  bool d_inUnsatCore;
public:
  CheckSatCommand() throw();
  CheckSatCommand(const Expr& expr, bool inUnsatCore = true) throw();
  ~CheckSatCommand() throw() {}
  Expr getExpr() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Result getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class CheckSatCommand */

class CVC4_PUBLIC QueryCommand : public Command {
protected:
  Expr d_expr;
  Result d_result;
  bool d_inUnsatCore;
public:
  QueryCommand(const Expr& e, bool inUnsatCore = true) throw();
  ~QueryCommand() throw() {}
  Expr getExpr() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Result getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class QueryCommand */

// this is TRANSFORM in the CVC presentation language
class CVC4_PUBLIC SimplifyCommand : public Command {
protected:
  Expr d_term;
  Expr d_result;
public:
  SimplifyCommand(Expr term) throw();
  ~SimplifyCommand() throw() {}
  Expr getTerm() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Expr getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class SimplifyCommand */

class CVC4_PUBLIC ExpandDefinitionsCommand : public Command {
protected:
  Expr d_term;
  Expr d_result;
public:
  ExpandDefinitionsCommand(Expr term) throw();
  ~ExpandDefinitionsCommand() throw() {}
  Expr getTerm() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Expr getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class ExpandDefinitionsCommand */

class CVC4_PUBLIC GetValueCommand : public Command {
protected:
  std::vector<Expr> d_terms;
  Expr d_result;
public:
  GetValueCommand(Expr term) throw();
  GetValueCommand(const std::vector<Expr>& terms) throw();
  ~GetValueCommand() throw() {}
  const std::vector<Expr>& getTerms() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Expr getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class GetValueCommand */

class CVC4_PUBLIC GetAssignmentCommand : public Command {
protected:
  SExpr d_result;
public:
  GetAssignmentCommand() throw();
  ~GetAssignmentCommand() throw() {}
  void invoke(SmtEngine* smtEngine) throw();
  SExpr getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class GetAssignmentCommand */

class CVC4_PUBLIC GetModelCommand : public Command {
protected:
  Model* d_result;
  SmtEngine* d_smtEngine;
public:
  GetModelCommand() throw();
  ~GetModelCommand() throw() {}
  void invoke(SmtEngine* smtEngine) throw();
  // Model is private to the library -- for now
  //Model* getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class GetModelCommand */

class CVC4_PUBLIC GetProofCommand : public Command {
protected:
  Proof* d_result;
public:
  GetProofCommand() throw();
  ~GetProofCommand() throw() {}
  void invoke(SmtEngine* smtEngine) throw();
  Proof* getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class GetProofCommand */

class CVC4_PUBLIC GetInstantiationsCommand : public Command {
protected:
  //Instantiations* d_result;
  SmtEngine* d_smtEngine;
public:
  GetInstantiationsCommand() throw();
  ~GetInstantiationsCommand() throw() {}
  void invoke(SmtEngine* smtEngine) throw();
  //Instantiations* getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class GetInstantiationsCommand */

class CVC4_PUBLIC GetUnsatCoreCommand : public Command {
protected:
  UnsatCore d_result;
  std::map<Expr, std::string> d_names;
public:
  GetUnsatCoreCommand() throw();
  GetUnsatCoreCommand(const std::map<Expr, std::string>& names) throw();
  ~GetUnsatCoreCommand() throw() {}
  void invoke(SmtEngine* smtEngine) throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  const UnsatCore& getUnsatCore() const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class GetUnsatCoreCommand */

class CVC4_PUBLIC GetAssertionsCommand : public Command {
protected:
  std::string d_result;
public:
  GetAssertionsCommand() throw();
  ~GetAssertionsCommand() throw() {}
  void invoke(SmtEngine* smtEngine) throw();
  std::string getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class GetAssertionsCommand */

class CVC4_PUBLIC SetBenchmarkStatusCommand : public Command {
protected:
  BenchmarkStatus d_status;
public:
  SetBenchmarkStatusCommand(BenchmarkStatus status) throw();
  ~SetBenchmarkStatusCommand() throw() {}
  BenchmarkStatus getStatus() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class SetBenchmarkStatusCommand */

class CVC4_PUBLIC SetBenchmarkLogicCommand : public Command {
protected:
  std::string d_logic;
public:
  SetBenchmarkLogicCommand(std::string logic) throw();
  ~SetBenchmarkLogicCommand() throw() {}
  std::string getLogic() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class SetBenchmarkLogicCommand */

class CVC4_PUBLIC SetInfoCommand : public Command {
protected:
  std::string d_flag;
  SExpr d_sexpr;
public:
  SetInfoCommand(std::string flag, const SExpr& sexpr) throw();
  ~SetInfoCommand() throw() {}
  std::string getFlag() const throw();
  SExpr getSExpr() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class SetInfoCommand */

class CVC4_PUBLIC GetInfoCommand : public Command {
protected:
  std::string d_flag;
  std::string d_result;
public:
  GetInfoCommand(std::string flag) throw();
  ~GetInfoCommand() throw() {}
  std::string getFlag() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  std::string getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class GetInfoCommand */

class CVC4_PUBLIC SetOptionCommand : public Command {
protected:
  std::string d_flag;
  SExpr d_sexpr;
public:
  SetOptionCommand(std::string flag, const SExpr& sexpr) throw();
  ~SetOptionCommand() throw() {}
  std::string getFlag() const throw();
  SExpr getSExpr() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class SetOptionCommand */

class CVC4_PUBLIC GetOptionCommand : public Command {
protected:
  std::string d_flag;
  std::string d_result;
public:
  GetOptionCommand(std::string flag) throw();
  ~GetOptionCommand() throw() {}
  std::string getFlag() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  std::string getResult() const throw();
  void printResult(std::ostream& out, uint32_t verbosity = 2) const throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class GetOptionCommand */

class CVC4_PUBLIC DatatypeDeclarationCommand : public Command {
private:
  std::vector<DatatypeType> d_datatypes;
public:
  DatatypeDeclarationCommand(const DatatypeType& datatype) throw();
  ~DatatypeDeclarationCommand() throw() {}
  DatatypeDeclarationCommand(const std::vector<DatatypeType>& datatypes) throw();
  const std::vector<DatatypeType>& getDatatypes() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class DatatypeDeclarationCommand */

class CVC4_PUBLIC RewriteRuleCommand : public Command {
public:
  typedef std::vector< std::vector< Expr > > Triggers;
protected:
  typedef std::vector< Expr > VExpr;
  VExpr d_vars;
  VExpr d_guards;
  Expr d_head;
  Expr d_body;
  Triggers d_triggers;
public:
  RewriteRuleCommand(const std::vector<Expr>& vars,
                     const std::vector<Expr>& guards,
                     Expr head,
                     Expr body,
                     const Triggers& d_triggers) throw();
  RewriteRuleCommand(const std::vector<Expr>& vars,
                     Expr head,
                     Expr body) throw();
  ~RewriteRuleCommand() throw() {}
  const std::vector<Expr>& getVars() const throw();
  const std::vector<Expr>& getGuards() const throw();
  Expr getHead() const throw();
  Expr getBody() const throw();
  const Triggers& getTriggers() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class RewriteRuleCommand */

class CVC4_PUBLIC PropagateRuleCommand : public Command {
public:
  typedef std::vector< std::vector< Expr > > Triggers;
protected:
  typedef std::vector< Expr > VExpr;
  VExpr d_vars;
  VExpr d_guards;
  VExpr d_heads;
  Expr d_body;
  Triggers d_triggers;
  bool d_deduction;
public:
  PropagateRuleCommand(const std::vector<Expr>& vars,
                       const std::vector<Expr>& guards,
                       const std::vector<Expr>& heads,
                       Expr body,
                       const Triggers& d_triggers,
                       /* true if we want a deduction rule */
                       bool d_deduction = false) throw();
  PropagateRuleCommand(const std::vector<Expr>& vars,
                       const std::vector<Expr>& heads,
                       Expr body,
                       bool d_deduction = false) throw();
  ~PropagateRuleCommand() throw() {}
  const std::vector<Expr>& getVars() const throw();
  const std::vector<Expr>& getGuards() const throw();
  const std::vector<Expr>& getHeads() const throw();
  Expr getBody() const throw();
  const Triggers& getTriggers() const throw();
  bool isDeduction() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class PropagateRuleCommand */


class CVC4_PUBLIC QuitCommand : public Command {
public:
  QuitCommand() throw();
  ~QuitCommand() throw() {}
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class QuitCommand */

class CVC4_PUBLIC CommentCommand : public Command {
  std::string d_comment;
public:
  CommentCommand(std::string comment) throw();
  ~CommentCommand() throw() {}
  std::string getComment() const throw();
  void invoke(SmtEngine* smtEngine) throw();
  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class CommentCommand */

class CVC4_PUBLIC CommandSequence : public Command {
private:
  /** All the commands to be executed (in sequence) */
  std::vector<Command*> d_commandSequence;
  /** Next command to be executed */
  unsigned int d_index;
public:
  CommandSequence() throw();
  ~CommandSequence() throw();

  void addCommand(Command* cmd) throw();
  void clear() throw();

  void invoke(SmtEngine* smtEngine) throw();
  void invoke(SmtEngine* smtEngine, std::ostream& out) throw();

  typedef std::vector<Command*>::iterator iterator;
  typedef std::vector<Command*>::const_iterator const_iterator;

  const_iterator begin() const throw();
  const_iterator end() const throw();

  iterator begin() throw();
  iterator end() throw();

  Command* exportTo(ExprManager* exprManager, ExprManagerMapCollection& variableMap);
  Command* clone() const;
  std::string getCommandName() const throw();
};/* class CommandSequence */

class CVC4_PUBLIC DeclarationSequence : public CommandSequence {
public:
  ~DeclarationSequence() throw() {}
};/* class DeclarationSequence */

}/* CVC4 namespace */

#endif /* __CVC4__COMMAND_H */
