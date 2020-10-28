/*********************                                                        */
/*! \file lfsc_printer.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Andrew Reynolds
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2020 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved.  See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief The module for printing Lfsc proof nodes
 **/

#include "proof/lfsc/lfsc_printer.h"

#include "expr/node_algorithm.h"
#include "proof/lfsc/letify.h"

namespace CVC4 {
namespace proof {

 LfscPrinter::LfscPrinter() : d_lcb(), d_tproc(&d_lcb){}
 
void LfscPrinter::print(std::ostream& out,
                        const std::vector<Node>& assertions,
                        const ProofNode* pn)
{
  // closing parentheses
  std::stringstream cparen;

  // [1] compute and print the declarations
  std::unordered_set<Node, NodeHashFunction> syms;
  std::unordered_set<TNode, TNodeHashFunction> visited;
  std::vector<Node> iasserts;
  for (const Node& a : assertions)
  {
    expr::getSymbols(a, syms, visited);
    iasserts.push_back(d_tproc.toInternal(a));
  }
  // [1a] user declared sorts
  std::unordered_set<TypeNode, TypeNodeHashFunction> sts;
  for (const Node& s : syms)
  {
    TypeNode st = s.getType();
    if (st.isSort() && sts.find(st) == sts.end())
    {
      sts.insert(st);
      out << "(declare " << st << " sort)" << std::endl;
    }
  }
  // [1b] user declare function symbols
  for (const Node& s : syms)
  {
    out << "(declare " << s << " ";
    print(out, s.getType());
    out << ")";
  }

  // [2] print the check command and term lets
  out << "(check" << std::endl;
  cparen << ")";
  // compute the term lets
  std::vector<Node> visitList;
  std::map<Node, uint32_t> count;
  for (const Node& ia : iasserts)
  {
    Letify::updateCounts(ia, visitList, count);
  }
  uint32_t counter = 0;
  std::vector<Node> letList;
  std::map<Node, uint32_t> letMap;
  Letify::convertCountToLet(visitList, count, letList, letMap, counter);
  printLetList(out, cparen, letList, letMap);

  // [3] print the assertions, with letification
  // the assumption identifier mapping
  std::map<Node, uint32_t> passumeMap;
  for (size_t i = 0, nasserts = iasserts.size(); i < nasserts; i++)
  {
    Node ia = iasserts[i];
    out << "(% ";
    printAssumeId(out, i);
    out << " ";
    printInternal(out, ia, letMap);
    out << std::endl;
    cparen << ")";
    // remember the assumption name
    passumeMap[ia] = i;
  }

  // [4] print the annotation
  out << "(: (holds false)" << std::endl;
  cparen << ")";

  // [5] print the proof body
  printProofLetify(out, pn, letMap, passumeMap);

  out << cparen.str();
}

void LfscPrinter::print(std::ostream& out, const ProofNode* pn)
{
  // TODO: compute term lets?
  std::map<Node, uint32_t> letMap;
  // empty passume map
  std::map<Node, uint32_t> passumeMap;
  printProofLetify(out, pn, letMap, passumeMap);
}

void LfscPrinter::printProofLetify(std::ostream& out,
                                   const ProofNode* pn,
                                   std::map<Node, uint32_t>& letMap,
                                   std::map<Node, uint32_t>& passumeMap)
{
  // closing parentheses
  std::stringstream cparen;

  // [1] compute and print the proof lets
  uint32_t pcounter = 0;
  std::vector<const ProofNode*> pletList;
  std::map<const ProofNode*, uint32_t> pletMap;
  Letify::computeProofLet(pn, pletList, pletMap, pcounter);
  // define the let proofs
  out << "; Let proofs:" << std::endl;
  std::map<const ProofNode*, uint32_t>::iterator itp;
  for (const ProofNode* p : pletList)
  {
    itp = pletMap.find(p);
    Assert(itp != pletMap.end());
    out << "(plet _ _ ";
    printProofInternal(out, p, letMap, pletMap, passumeMap);
    out << " (\\ ";
    printProofId(out, itp->second);
    out << std::endl;
    cparen << "))";
  }
  out << std::endl;

  // [2] print the proof body
  printProofInternal(out, pn, letMap, pletMap, passumeMap);

  out << cparen.str();
}

void LfscPrinter::printProofInternal(
    std::ostream& out,
    const ProofNode* pn,
    std::map<Node, uint32_t>& letMap,
    std::map<const ProofNode*, uint32_t>& pletMap,
    std::map<Node, uint32_t>& passumeMap)
{
  // the stack
  std::vector<PExpr> visit;
  // whether we have process children
  std::map<const ProofNode*, bool> processedChildren;
  // helper iterators
  std::map<const ProofNode*, bool>::iterator pit;
  std::map<const ProofNode*, uint32_t>::iterator pletIt;
  std::map<Node, uint32_t>::iterator passumeIt;
  Node curn;
  const ProofNode* cur;
  visit.push_back(PExpr(pn));
  do
  {
    curn = visit.back().d_node;
    cur = visit.back().d_pnode;
    visit.pop_back();
    // case 1: printing a proof
    if (cur != nullptr)
    {
      pit = processedChildren.find(cur);
      if (pit == processedChildren.end())
      {
        // maybe it is letified
        pletIt = pletMap.find(cur);
        if (pletIt != pletMap.end())
        {
          // a letified proof
          printProofId(out, pletIt->second);
        }
        else if (cur->getRule() == PfRule::ASSUME)
        {
          // an assumption, must have a name
          passumeIt = passumeMap.find(cur->getResult());
          Assert(passumeIt != passumeMap.end());
          printAssumeId(out, passumeIt->second);
        }
        else
        {
          // a normal rule application, compute the proof arguments
          processedChildren[cur] = false;
          computeProofArgs(cur, visit);
          // print the rule name
          out << "(";
          printRule(out, cur);
        }
      }
      else if (!pit->second)
      {
        processedChildren[cur] = true;
        out << ")" << std::endl;
      }
    }
    // case 2: printing a node
    else if (!curn.isNull())
    {
      printInternal(out, curn, letMap);
    }
    // case 3: a hole
    else
    {
      out << "_ ";
    }
  } while (!visit.empty());
}

void LfscPrinter::computeProofArgs(const ProofNode* pn,
                                   std::vector<PExpr>& pargs)
{
  // TODO
}

void LfscPrinter::print(std::ostream& out, Node n)
{
  Node ni = d_tproc.toInternal(n);
  printLetify(out, ni);
}

void LfscPrinter::printLetify(std::ostream& out, Node n)
{
  // closing parentheses
  std::stringstream cparen;

  std::vector<Node> letList;
  std::map<Node, uint32_t> letMap;
  uint32_t counter = 0;
  Letify::computeLet(n, letList, letMap, counter);

  // [1] print the letification
  printLetList(out, cparen, letList, letMap);

  // [2] print the body
  printInternal(out, n, letMap);

  out << cparen.str();
}

void LfscPrinter::printLetList(std::ostream& out, std::ostream& cparen, 
                  const std::vector<Node>& letList,
                  const std::map<Node, uint32_t>& letMap)
{
  std::map<Node, uint32_t>::const_iterator it;
  for (size_t i = 0, nlets = letList.size(); i < nlets; i++)
  {
    Node nl = letList[i];
    it = letMap.find(nl);
    Assert(it != letMap.end());
    out << "(@ ";
    printId(out, it->second);
    out << " ";
    printInternal(out, nl, letMap);
    out << std::endl;
    cparen << ")";
  }
}

void LfscPrinter::printInternal(std::ostream& out,
                                Node n,
                                const std::map<Node, uint32_t>& letMap)
{
  // TODO: dag thresh 0 print?
  out << Letify::convert(n, letMap, "@t");
}

void LfscPrinter::print(std::ostream& out, TypeNode tn) {
  TypeNode tni = d_tproc.toInternalType(tn);
  printInternal(out, tni);
}

void LfscPrinter::printInternal(std::ostream& out, TypeNode tn) {
  // types are always printed as-is
  out << tn;
}

void LfscPrinter::printRule(std::ostream& out, const ProofNode* pn)
{
  // TODO: proper conversion
  out << pn->getRule();
}

void LfscPrinter::printId(std::ostream& out, uint32_t id) { out << "@t" << id; }

void LfscPrinter::printProofId(std::ostream& out, uint32_t id)
{
  out << "@p" << id;
}

void LfscPrinter::printAssumeId(std::ostream& out, uint32_t id)
{
  out << "@a" << id;
}

}  // namespace proof
}  // namespace CVC4
