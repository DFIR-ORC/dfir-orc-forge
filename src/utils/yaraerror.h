//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include <string>
#include <system_error>

namespace YaraError {

enum YaraErrorCode {
    Success = 0,
    InsufficientMemory,
    InvalidFile,
    InvalidHandle,
    ScanTimeout,
    CompilerError,
    SyntaxError,
    TooManyScanThreads,
    CallbackError,
    InvalidArgument,
    DuplicatedRuleIdentifier,
    DuplicatedTagIdentifier,
    DuplicatedMetaIdentifier,
    DuplicatedStringIdentifier,
    UnreferencedString,
    UndefinedString,
    IncludeDepthExceeded,
    CircularInclude,
    IncludeFileNotFound,
    WrongType,
    IndexOutOfBounds,
    RegularExpressionError,
    UnexpectedCharacter,
    UnexpectedEndOfFile,
    NestedComments,
    UnexpectedToken,
    UnexpectedEOF,
    TooManyStrings,
    TooManyRules,
    TooManyMatches,
    TooManyThreads,
    TooManyFiles,
    TooManyModules,
    TooManyAtoms,
    TooManyVariables,
    TooManyNamespaces,
    TooManyImports,
    TooManyIncludes,
    TooManyStructures,
    TooManyUnions,
    TooManyEnums,
    TooManyFunctions,
    TooManyGlobals,
    TooManyLoopVariables,
    TooManyParameters,
    TooManyArguments,
    TooManyLocals,
    TooManyExpressions,
    TooManyStatements,
    TooManyBlocks
};

class YaraErrorCategory : public std::error_category
{
public:
    const char *name() const noexcept override { return "YaraError"; }

    std::string message(int ev) const override
    {
        switch (static_cast<YaraErrorCode>(ev)) {
        case Success:
            return "The operation completed successfully.";
        case InsufficientMemory:
            return "Insufficient memory to complete the operation.";
        case InvalidFile:
            return "The file cannot be opened or read.";
        case InvalidHandle:
            return "The handle is invalid.";
        case ScanTimeout:
            return "The scan operation timed out.";
        case CompilerError:
            return "There was an error during the compilation of the rules.";
        case SyntaxError:
            return "There is a syntax error in the YARA rules.";
        case TooManyScanThreads:
            return "Too many scan threads are running.";
        case CallbackError:
            return "An error occurred in a callback function.";
        case InvalidArgument:
            return "An invalid argument was provided to a function.";
        case DuplicatedRuleIdentifier:
            return "A rule identifier is duplicated.";
        case DuplicatedTagIdentifier:
            return "A tag identifier is duplicated.";
        case DuplicatedMetaIdentifier:
            return "A meta identifier is duplicated.";
        case DuplicatedStringIdentifier:
            return "A string identifier is duplicated.";
        case UnreferencedString:
            return "A string is defined but not referenced in any rule.";
        case UndefinedString:
            return "A string is referenced but not defined.";
        case IncludeDepthExceeded:
            return "The maximum include depth has been exceeded.";
        case CircularInclude:
            return "A circular include has been detected.";
        case IncludeFileNotFound:
            return "An include file was not found.";
        case WrongType:
            return "A value of the wrong type was provided.";
        case IndexOutOfBounds:
            return "An index is out of bounds.";
        case RegularExpressionError:
            return "There was an error in a regular expression.";
        case UnexpectedCharacter:
            return "An unexpected character was encountered.";
        case UnexpectedEndOfFile:
            return "The end of the file was reached unexpectedly.";
        case NestedComments:
            return "Nested comments are not allowed.";
        case UnexpectedToken:
            return "An unexpected token was encountered.";
        case UnexpectedEOF:
            return "The end of the file was reached unexpectedly.";
        case TooManyStrings:
            return "Too many strings are defined.";
        case TooManyRules:
            return "Too many rules are defined.";
        case TooManyMatches:
            return "Too many matches were found.";
        case TooManyThreads:
            return "Too many threads are running.";
        case TooManyFiles:
            return "Too many files are open.";
        case TooManyModules:
            return "Too many modules are loaded.";
        case TooManyAtoms:
            return "Too many atoms are defined.";
        case TooManyVariables:
            return "Too many variables are defined.";
        case TooManyNamespaces:
            return "Too many namespaces are defined.";
        case TooManyImports:
            return "Too many imports are defined.";
        case TooManyIncludes:
            return "Too many includes are defined.";
        case TooManyStructures:
            return "Too many structures are defined.";
        case TooManyUnions:
            return "Too many unions are defined.";
        case TooManyEnums:
            return "Too many enums are defined.";
        case TooManyFunctions:
            return "Too many functions are defined.";
        case TooManyGlobals:
            return "Too many globals are defined.";
        case TooManyLoopVariables:
            return "Too many loop variables are defined.";
        case TooManyParameters:
            return "Too many parameters are defined.";
        case TooManyArguments:
            return "Too many arguments are provided.";
        case TooManyLocals:
            return "Too many locals are defined.";
        case TooManyExpressions:
            return "Too many expressions are defined.";
        case TooManyStatements:
            return "Too many statements are defined.";
        case TooManyBlocks:
            return "Too many blocks are defined.";
        default:
            return "Unknown YARA error.";
        }
    }
};

const YaraErrorCategory &yaraCategory()
{
    static YaraErrorCategory instance;
    return instance;
}

std::error_code make_error_code(int error)
{
    return {error, yaraCategory()};
}

} // namespace YaraError
