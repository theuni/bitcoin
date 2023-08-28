// Copyright (c) 2023 Bitcoin Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "serparams.h"

#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

namespace bitcoin {

void SerParamsCheck::registerMatchers(clang::ast_matchers::MatchFinder* finder)
{
    using namespace clang::ast_matchers;

    finder->addMatcher(traverse(clang::TK_IgnoreUnlessSpelledInSource,
        varDecl(
            hasType(hasCanonicalType(hasDeclaration(cxxRecordDecl(isSameOrDerivedFrom(hasName("SerParams")))))),
            hasInitializer(
                initListExpr(anyOf(
                    hasInit(0, anyOf(initListExpr(), implicitValueInitExpr())),
                    hasInit(1, anyOf(initListExpr(), implicitValueInitExpr())),
                    hasInit(2, anyOf(initListExpr(), implicitValueInitExpr())),
                    hasInit(3, anyOf(initListExpr(), implicitValueInitExpr())),
                    hasInit(4, anyOf(initListExpr(), implicitValueInitExpr())),
                    hasInit(5, anyOf(initListExpr(), implicitValueInitExpr())))
                ).bind("initexpr")
            )
        )), this);
}

void SerParamsCheck::check(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    if (const clang::InitListExpr* expr = Result.Nodes.getNodeAs<clang::InitListExpr>("initexpr")) {
        const auto user_diag = diag(expr->getEndLoc(), "Unitialized SerParams");
    }
}

} // namespace bitcoin
