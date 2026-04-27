/**********************************************************************************************

    rlparser - raylib header API parser, extracts API information as separate tokens

    This parser scans raylib.h to get API information about defines, structs, aliases, enums, callbacks and functions.
    All data is divided into pieces, usually as strings. The following types are used for data:
     - struct DefineInfo
     - struct StructInfo
     - struct AliasInfo
     - struct EnumInfo
     - struct FunctionInfo

    WARNING: This parser is specifically designed to work with raylib.h, and has some contraints
    in that regards. Still, it can also work with other header files that follow same file structure
    conventions as raylib.h: rlgl.h, raymath.h, raygui.h, reasings.h

    CONSTRAINTS:
    This parser is specifically designed to work with raylib.h, so, it has some constraints:

     - Functions are expected as a single line with the following structure:
       <retType> <name>(<paramType[0]> <paramName[0]>, <paramType[1]> <paramName[1]>);  <desc>

       WARNING: Be careful with functions broken into several lines, it breaks the process!

     - Structures are expected as several lines with the following form:
       <desc>
       typedef struct <name> {
           <fieldType[0]> <fieldName[0]>;  <fieldDesc[0]>
           <fieldType[1]> <fieldName[1]>;  <fieldDesc[1]>
           <fieldType[2]> <fieldName[2]>;  <fieldDesc[2]>
       } <name>;

     - Enums are expected as several lines with the following form:
       <desc>
       typedef enum {
           <valueName[0]> = <valueInteger[0]>, <valueDesc[0]>
           <valueName[1]>,
           <valueName[2]>, <valueDesc[2]>
           <valueName[3]>  <valueDesc[3]>
       } <name>;

       NOTE: Multiple options are supported for enums:
          - If value is not provided, (<valueInteger[i -1]> + 1) is assigned
          - Value description can be provided or not

    OTHER NOTES:
     - This parser could work with other C header files if mentioned constraints are followed.
     - This parser does not require <string.h> library, all data is parsed directly from char buffers.

    LICENSE: zlib/libpng

    raylib-parser is licensed under an unmodified zlib/libpng license, which is an OSI-certified,
    BSD-like license that allows static linking with closed source software:

    Copyright (c) 2021-2026 Ramon Santamaria (@raysan5)

    Contributions:
        Mojo bindings generator by @kivicode

**********************************************************************************************/

#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>  // Required for: malloc(), calloc(), realloc(), free(), atoi(), strtol()
#include <stdio.h>   // Required for: printf(), fopen(), fseek(), ftell(), fread(), fclose()
#include <stdbool.h> // Required for: bool
#include <ctype.h>   // Required for: isdigit()

#define MAX_DEFINES_TO_PARSE 2048 // Maximum number of defines to parse
#define MAX_STRUCTS_TO_PARSE 64   // Maximum number of structures to parse
#define MAX_ALIASES_TO_PARSE 64   // Maximum number of aliases to parse
#define MAX_ENUMS_TO_PARSE 64     // Maximum number of enums to parse
#define MAX_CALLBACKS_TO_PARSE 64 // Maximum number of callbacks to parse
#define MAX_FUNCS_TO_PARSE 1024   // Maximum number of functions to parse

#define MAX_LINE_LENGTH 1024 // Maximum length of one line (including comments)

#define MAX_STRUCT_FIELDS 64       // Maximum number of struct fields
#define MAX_ENUM_VALUES 512        // Maximum number of enum values
#define MAX_FUNCTION_PARAMETERS 12 // Maximum number of function parameters

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Define value type
typedef enum
{
    UNKNOWN = 0,
    MACRO,
    GUARD,
    INT,
    INT_MATH,
    LONG,
    LONG_MATH,
    FLOAT,
    FLOAT_MATH,
    DOUBLE,
    DOUBLE_MATH,
    CHAR,
    STRING,
    COLOR
} DefineType;

// Define info data
typedef struct DefineInfo
{
    char name[64];   // Define name
    int type;        // Define type: enum DefineType
    char value[256]; // Define value
    char desc[128];  // Define description
    bool isHex;      // Define is hex number (for types INT, LONG)
} DefineInfo;

// Struct info data
typedef struct StructInfo
{
    char name[64];                          // Struct name
    char desc[128];                         // Struct type description
    int fieldCount;                         // Number of fields in the struct
    char fieldType[MAX_STRUCT_FIELDS][64];  // Field type
    char fieldName[MAX_STRUCT_FIELDS][64];  // Field name
    char fieldDesc[MAX_STRUCT_FIELDS][128]; // Field description
} StructInfo;

// Alias info data
typedef struct AliasInfo
{
    char type[64];  // Alias type
    char name[64];  // Alias name
    char desc[128]; // Alias description
} AliasInfo;

// Enum info data
typedef struct EnumInfo
{
    char name[64];                        // Enum name
    char desc[128];                       // Enum description
    int valueCount;                       // Number of values in enumerator
    char valueName[MAX_ENUM_VALUES][64];  // Value name definition
    int valueInteger[MAX_ENUM_VALUES];    // Value integer
    char valueDesc[MAX_ENUM_VALUES][128]; // Value description
} EnumInfo;

// Function info data
typedef struct FunctionInfo
{
    char name[64];                                // Function name
    char desc[512];                               // Function description (comment at the end)
    char retType[32];                             // Return value type
    int paramCount;                               // Number of function parameters
    char paramType[MAX_FUNCTION_PARAMETERS][32];  // Parameters type
    char paramName[MAX_FUNCTION_PARAMETERS][32];  // Parameters name
    char paramDesc[MAX_FUNCTION_PARAMETERS][128]; // Parameters description
} FunctionInfo;

// Output format for parsed data
typedef enum
{
    DEFAULT = 0,
    JSON,
    XML,
    LUA,
    CODE
} OutputFormat;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static int defineCount = 0;
static int structCount = 0;
static int aliasCount = 0;
static int enumCount = 0;
static int callbackCount = 0;
static int funcCount = 0;
static DefineInfo *defines = NULL;
static StructInfo *structs = NULL;
static AliasInfo *aliases = NULL;
static EnumInfo *enums = NULL;
static FunctionInfo *callbacks = NULL;
static FunctionInfo *funcs = NULL;

// Command line variables
static char apiDefine[32] = {0};  // Functions define (i.e. RLAPI for raylib.h, RMDEF for raymath.h, etc.)
static char truncAfter[32] = {0}; // Truncate marker (i.e. "RLGL IMPLEMENTATION" for rlgl.h)
static int outputFormat = DEFAULT;

// NOTE: Filename max length depends on OS, in Windows MAX_PATH = 256
static char inFileName[512] = {0};  // Input file name (required in case of drag & drop over executable)
static char outFileName[512] = {0}; // Output file name (required for file save/export)

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void ShowCommandLineInfo(void);                  // Show command line usage info
static void ProcessCommandLine(int argc, char *argv[]); // Process command line input

static char *LoadFileText(const char *fileName, int *length);                                  // Load text file - UnloadFileText() required!
static void UnloadFileText(char *text);                                                        // Unload text data
static char **LoadTextLines(const char *buffer, int length, int *lineCount);                   // Load all lines from a text buffer (expecting lines ending with '\n') - UnloadTextLines() required
static void UnloadTextLines(char **lines, int lineCount);                                      // Unload text lines data
static void GetDataTypeAndName(const char *typeName, int typeNameLen, char *type, char *name); // Get data type and name from a string containing both (i.e function param and struct fields)
static void GetDescription(const char *source, char *description);                             // Get description comment from a line, do nothing if no comment in line
static void MoveArraySize(char *name, char *type);                                             // Move array size from name to type
static unsigned int TextLength(const char *text);                                              // Get text length in bytes, check for \0 character
static bool IsTextEqual(const char *text1, const char *text2, unsigned int count);
static int TextFindIndex(const char *text, const char *find);            // Find first text occurrence within a string
static void MemoryCopy(void *dest, const void *src, unsigned int count); // Memory copy, memcpy() replacement to avoid <string.h>
static char *EscapeBackslashes(char *text);                              // Replace '\' by "\\" when exporting to JSON and XML
static const char *StrDefineType(DefineType type);                       // Get string of define type

static void ExportParsedData(const char *fileName, int format); // Export parsed data in desired format
static void ExportCodeBundle(const char *rootDir);              // Export direct Mojo/C codegen bundle
static void ExportMojoTypes(const char *rootDir);
static void ExportMojoPublicTypes(const char *rootDir);
static void ExportMojoRawModule(const char *rootDir, const char *moduleName, const char *prefix);
static void ExportMojoRawInit(const char *rootDir);
static void ExportMojoSafe(const char *rootDir);
static void ExportMojoRaymathSafe(const char *rootDir);
static void ExportMojoPackageInit(const char *rootDir);
static void ExportNativeShim(const char *rootDir);
static void EnsureCodegenDirectories(const char *rootDir);
static void EnsureDirectory(const char *dirPath);
static void JoinPath(const char *base, const char *leaf, char *outPath, int outPathSize);
static void CopyText(char *dst, const char *src, int dstSize);
static void CopyTrimmed(char *dst, const char *src, int dstSize);
static void ToSnakeCase(const char *source, char *outText, int outTextSize);
static bool StartsWith(const char *text, const char *prefix);
static int CountCharOccurrences(const char *text, char ch);
static bool IsRaymathApi(void);
static bool IsKnownStructOrAlias(const char *typeName);
static bool IsVoidType(const char *typeName);
static bool IsUnsupportedFunction(const FunctionInfo *func);
static bool IsMutablePointerType(const char *typeName);
static bool IsIntegralCountType(const char *typeName);
static bool IsCountLikeName(const char *name);
static bool FunctionHasMemFreeOwnership(const FunctionInfo *func);
static int FindFunctionIndexByName(const char *name);
static int FindMatchingReleaseFunction(const FunctionInfo *func, char *releaseName, int releaseNameSize);
static void GetOwnedStructName(const FunctionInfo *func, char *structName, int structNameSize);
static void GetMojoType(const char *cType, bool forReturnType, bool forCallbackParam, char *outType, int outTypeSize);
static void GetMojoPublicType(const char *cType, bool forReturnType, bool forCallbackParam, char *outType, int outTypeSize);
static void GetMojoLayoutType(const char *cType, char *outType, int outTypeSize);
static bool GetPointerBaseType(const char *typeName, char *baseType, int baseTypeSize, bool *isConst, int *pointerCount);
static void ResolveStructOrAliasName(const char *typeName, char *outTypeName, int outTypeNameSize);
static bool IsPointerAlias(const char *typeName);
static bool GetSpanCompanionIndex(const FunctionInfo *func, int paramIndex, int *countIndex);
static bool ShouldUseMutValueParam(const FunctionInfo *func, int paramIndex, char *pointeeType, int pointeeTypeSize);
static bool ShouldUseRefValueParam(const FunctionInfo *func, int paramIndex, char *pointeeType, int pointeeTypeSize);
static void GetMojoPublicSpanType(const char *pointerType, char *outType, int outTypeSize);
static void GetMojoAliasTarget(const AliasInfo *alias, char *outType, int outTypeSize);
static void SanitizeComment(const char *source, char *outComment, int outCommentSize);
static void GetRelativeSourceName(const char *sourceName, char *outSourceName, int outSourceNameSize);
static void WriteGeneratedHeader(FILE *outFile, const char *title, const char *sourceName);
static void WriteMojoCommonImports(FILE *outFile);
static void WriteMojoDocstring(FILE *outFile, const char *comment, int indentLevel);
static void WriteMojoSignatureParams(FILE *outFile, const FunctionInfo *func, bool forCallbackParam);
static void WriteMojoCallArgs(FILE *outFile, const FunctionInfo *func);
static void WritePublicToRawArg(FILE *outFile, const char *cType, const char *expr);
static void WriteRawToPublicExpr(FILE *outFile, const char *cType, const char *expr);
static void WritePublicTypeToRawExpr(FILE *outFile, const char *typeName, const char *expr);
static void WriteRawTypeToPublicExpr(FILE *outFile, const char *typeName, const char *expr);
static void WriteSpanPointerToRawExpr(FILE *outFile, const char *pointerType, const char *expr);

//----------------------------------------------------------------------------------
// Program main entry point
//----------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    if (argc > 1)
        ProcessCommandLine(argc, argv);

    const char *raylibhPath = "../../src/raylib.h\0";
    const char *raylibapiPath = "raylib_api.txt\0";
    const char *rlapiPath = "RLAPI\0";
    if (inFileName[0] == '\0')
        MemoryCopy(inFileName, raylibhPath, TextLength(raylibhPath) + 1);
    if (outFileName[0] == '\0')
        MemoryCopy(outFileName, raylibapiPath, TextLength(raylibapiPath) + 1);
    if (apiDefine[0] == '\0')
        MemoryCopy(apiDefine, rlapiPath, TextLength(rlapiPath) + 1);

    int length = 0;
    char *buffer = LoadFileText(inFileName, &length);

    if (buffer == NULL)
    {
        printf("Could not read input file: %s\n", inFileName);
        return 1;
    }

    // Preprocess buffer to get separate lines
    // NOTE: LoadTextLines() also removes leading spaces/tabs
    int lineCount = 0;
    char **lines = LoadTextLines(buffer, length, &lineCount);

    // Truncate lines (if required)
    if (truncAfter[0] != '\0')
    {
        int newCount = -1;
        for (int i = 0; i < lineCount; i++)
        {
            if (newCount > -1)
                free(lines[i]);
            else if (TextFindIndex(lines[i], truncAfter) > -1)
                newCount = i;
        }
        if (newCount > -1)
            lineCount = newCount;
        printf("Number of truncated text lines: %i\n", lineCount);
    }

    // Defines line indices
    int *defineLines = (int *)malloc(MAX_DEFINES_TO_PARSE * sizeof(int));

    // Structs line indices
    int *structLines = (int *)malloc(MAX_STRUCTS_TO_PARSE * sizeof(int));

    // Aliases line indices
    int *aliasLines = (int *)malloc(MAX_ALIASES_TO_PARSE * sizeof(int));

    // Enums line indices
    int *enumLines = (int *)malloc(MAX_ENUMS_TO_PARSE * sizeof(int));

    // Callbacks line indices
    int *callbackLines = (int *)malloc(MAX_CALLBACKS_TO_PARSE * sizeof(int));

    // Function line indices
    int *funcLines = (int *)malloc(MAX_FUNCS_TO_PARSE * sizeof(int));

    // Prepare required lines for parsing
    //----------------------------------------------------------------------------------
    // Read define lines
    for (int i = 0; i < lineCount; i++)
    {
        int j = 0;
        while ((lines[i][j] == ' ') || (lines[i][j] == '\t'))
            j++; // skip spaces and tabs in the beginning
        // Read define line
        if (IsTextEqual(lines[i] + j, "#define ", 8))
        {
            // Keep the line position in the array of lines,
            // so, we can scan that position and following lines
            defineLines[defineCount] = i;
            defineCount++;
        }
    }

    // Read struct lines
    for (int i = 0; i < lineCount; i++)
    {
        // Find structs
        // starting with "typedef struct ... {" or "typedef struct ... ; \n struct ... {"
        // ending with "} ... ;"
        // i.e. excluding "typedef struct rAudioBuffer rAudioBuffer;" -> Typedef and forward declaration only
        if (IsTextEqual(lines[i], "typedef struct", 14))
        {
            bool validStruct = IsTextEqual(lines[i + 1], "struct", 6);
            if (!validStruct)
            {
                for (int c = 0; c < MAX_LINE_LENGTH; c++)
                {
                    char v = lines[i][c];
                    if (v == '{')
                        validStruct = true;
                    if ((v == '{') || (v == ';') || (v == '\0'))
                        break;
                }
            }
            if (!validStruct)
                continue;
            structLines[structCount] = i;
            while (lines[i][0] != '}')
                i++;
            while (lines[i][0] != '\0')
                i++;
            structCount++;
        }
    }

    // Read alias lines
    for (int i = 0; i < lineCount; i++)
    {
        // Find aliases (lines with "typedef ... ...;")
        if (IsTextEqual(lines[i], "typedef", 7))
        {
            int spaceCount = 0;
            bool validAlias = false;

            for (int c = 0; c < MAX_LINE_LENGTH; c++)
            {
                char v = lines[i][c];
                if (v == ' ')
                    spaceCount++;
                if ((v == ';') && (spaceCount == 2))
                    validAlias = true;
                if ((v == ';') || (v == '(') || (v == '\0'))
                    break;
            }
            if (!validAlias)
                continue;
            aliasLines[aliasCount] = i;
            aliasCount++;
        }
    }

    // Read enum lines
    for (int i = 0; i < lineCount; i++)
    {
        // Read enum line
        if (IsTextEqual(lines[i], "typedef enum {", 14) && (lines[i][TextLength(lines[i]) - 1] != ';')) // ignore inline enums
        {
            // Keep the line position in the array of lines,
            // so, we can scan that position and following lines
            enumLines[enumCount] = i;
            enumCount++;
        }
    }

    // Read callback lines
    for (int i = 0; i < lineCount; i++)
    {
        // Find callbacks (lines with "typedef ... (* ... )( ... );")
        if (IsTextEqual(lines[i], "typedef", 7))
        {
            bool hasBeginning = false;
            bool hasMiddle = false;
            bool hasEnd = false;

            for (int c = 0; c < MAX_LINE_LENGTH; c++)
            {
                if ((lines[i][c] == '(') && (lines[i][c + 1] == '*'))
                    hasBeginning = true;
                if ((lines[i][c] == ')') && (lines[i][c + 1] == '('))
                    hasMiddle = true;
                if ((lines[i][c] == ')') && (lines[i][c + 1] == ';'))
                    hasEnd = true;
                if (hasEnd)
                    break;
            }

            if (hasBeginning && hasMiddle && hasEnd)
            {
                callbackLines[callbackCount] = i;
                callbackCount++;
            }
        }
    }

    // Read function lines
    for (int i = 0; i < lineCount; i++)
    {
        // Read function line (starting with `define`, i.e. for raylib.h "RLAPI")
        if (IsTextEqual(lines[i], apiDefine, TextLength(apiDefine)))
        {
            funcLines[funcCount] = i;
            funcCount++;
        }
    }

    // At this point we have all raylib defines, structs, aliases, enums, callbacks, functions lines data to start parsing

    UnloadFileText(buffer); // Unload text buffer

    // Parsing raylib data
    //----------------------------------------------------------------------------------
    // Define info data
    int defineIndex = 0;
    defines = (DefineInfo *)calloc(MAX_DEFINES_TO_PARSE, sizeof(DefineInfo));

    for (int i = 0; i < defineCount; i++)
    {
        char *linePtr = lines[defineLines[i]];
        int j = 0;

        while ((linePtr[j] == ' ') || (linePtr[j] == '\t'))
            j++; // Skip spaces and tabs in the beginning
        j += 8;  // Skip "#define "
        while ((linePtr[j] == ' ') || (linePtr[j] == '\t'))
            j++; // Skip spaces and tabs after "#define "

        // Extract name
        int defineNameStart = j;
        int openBraces = 0;
        while (linePtr[j] != '\0')
        {
            if (((linePtr[j] == ' ') || (linePtr[j] == '\t')) && (openBraces == 0))
                break;
            if (linePtr[j] == '(')
                openBraces++;
            if (linePtr[j] == ')')
                openBraces--;
            j++;
        }
        int defineNameEnd = j - 1;

        // Skip duplicates
        unsigned int nameLen = defineNameEnd - defineNameStart + 1;
        bool isDuplicate = false;
        for (int k = 0; k < defineIndex; k++)
        {
            if ((nameLen == TextLength(defines[k].name)) && IsTextEqual(defines[k].name, &linePtr[defineNameStart], nameLen))
            {
                isDuplicate = true;
                break;
            }
        }
        if (isDuplicate)
            continue;

        MemoryCopy(defines[defineIndex].name, &linePtr[defineNameStart], nameLen);

        // Determine type
        if (linePtr[defineNameEnd] == ')')
            defines[defineIndex].type = MACRO;

        while ((linePtr[j] == ' ') || (linePtr[j] == '\t'))
            j++; // Skip spaces and tabs after name

        int defineValueStart = j;
        if ((linePtr[j] == '\0') || (linePtr[j] == '/'))
            defines[defineIndex].type = GUARD;
        if (linePtr[j] == '"')
            defines[defineIndex].type = STRING;
        else if (linePtr[j] == '\'')
            defines[defineIndex].type = CHAR;
        else if (IsTextEqual(linePtr + j, "CLITERAL(Color)", 15))
            defines[defineIndex].type = COLOR;
        else if (isdigit(linePtr[j])) // Parsing numbers
        {
            bool isFloat = false, isNumber = true, isHex = false;
            while ((linePtr[j] != ' ') && (linePtr[j] != '\t') && (linePtr[j] != '\0'))
            {
                char ch = linePtr[j];
                if (ch == '.')
                    isFloat = true;
                if (ch == 'x')
                    isHex = true;
                if (!(isdigit(ch) ||
                      ((ch >= 'a') && (ch <= 'f')) ||
                      ((ch >= 'A') && (ch <= 'F')) ||
                      (ch == 'x') ||
                      (ch == 'L') ||
                      (ch == '.') ||
                      (ch == '+') ||
                      (ch == '-')))
                    isNumber = false;
                j++;
            }
            if (isNumber)
            {
                if (isFloat)
                {
                    defines[defineIndex].type = (linePtr[j - 1] == 'f') ? FLOAT : DOUBLE;
                }
                else
                {
                    defines[defineIndex].type = (linePtr[j - 1] == 'L') ? LONG : INT;
                    defines[defineIndex].isHex = isHex;
                }
            }
        }

        // Extracting value
        while ((linePtr[j] != '\\') && (linePtr[j] != '\0') && !((linePtr[j] == '/') && (linePtr[j + 1] == '/')))
            j++;
        int defineValueEnd = j - 1;
        while ((linePtr[defineValueEnd] == ' ') || (linePtr[defineValueEnd] == '\t'))
            defineValueEnd--; // Remove trailing spaces and tabs
        if ((defines[defineIndex].type == LONG) || (defines[defineIndex].type == FLOAT))
            defineValueEnd--; // Remove number postfix
        int valueLen = defineValueEnd - defineValueStart + 1;
        if (valueLen > 255)
            valueLen = 255;

        if (valueLen > 0)
            MemoryCopy(defines[defineIndex].value, &linePtr[defineValueStart], valueLen);

        // Extracting description
        if ((linePtr[j] == '/') && linePtr[j + 1] == '/')
        {
            j += 2;
            while (linePtr[j] == ' ')
                j++;
            int commentStart = j;
            while ((linePtr[j] != '\\') && (linePtr[j] != '\0'))
                j++;
            int commentEnd = j - 1;
            int commentLen = commentEnd - commentStart + 1;
            if (commentLen > 127)
                commentLen = 127;

            MemoryCopy(defines[defineIndex].desc, &linePtr[commentStart], commentLen);
        }

        // Parse defines of type UNKNOWN to find calculated numbers
        if (defines[defineIndex].type == UNKNOWN)
        {
            int largestType = UNKNOWN;
            bool isMath = true;
            char *valuePtr = defines[defineIndex].value;

            for (unsigned int c = 0; c < TextLength(valuePtr); c++)
            {
                char ch = valuePtr[c];

                // Skip operators and whitespace
                if ((ch == '(') ||
                    (ch == ')') ||
                    (ch == '+') ||
                    (ch == '-') ||
                    (ch == '*') ||
                    (ch == '/') ||
                    (ch == ' ') ||
                    (ch == '\t'))
                    continue;

                // Read number operand
                else if (isdigit(ch))
                {
                    bool isNumber = true, isFloat = false;
                    while (!((ch == '(') ||
                             (ch == ')') ||
                             (ch == '*') ||
                             (ch == '/') ||
                             (ch == ' ') ||
                             (ch == '\t') ||
                             (ch == '\0')))
                    {
                        if (ch == '.')
                            isFloat = true;
                        if (!(isdigit(ch) ||
                              ((ch >= 'a') && (ch <= 'f')) ||
                              ((ch >= 'A') && (ch <= 'F')) ||
                              (ch == 'x') ||
                              (ch == 'L') ||
                              (ch == '.') ||
                              (ch == '+') ||
                              (ch == '-')))
                        {
                            isNumber = false;
                            break;
                        }
                        c++;
                        ch = valuePtr[c];
                    }
                    if (isNumber)
                    {
                        // Found a valid number -> update largestType
                        int numberType;
                        if (isFloat)
                            numberType = (valuePtr[c - 1] == 'f') ? FLOAT_MATH : DOUBLE_MATH;
                        else
                            numberType = (valuePtr[c - 1] == 'L') ? LONG_MATH : INT_MATH;

                        if (numberType > largestType)
                            largestType = numberType;
                    }
                    else
                    {
                        isMath = false;
                        break;
                    }
                }
                else // Read string operand
                {
                    int operandStart = c;
                    while (!((ch == '\0') ||
                             (ch == ' ') ||
                             (ch == '(') ||
                             (ch == ')') ||
                             (ch == '+') ||
                             (ch == '-') ||
                             (ch == '*') ||
                             (ch == '/')))
                    {
                        c++;
                        ch = valuePtr[c];
                    }
                    int operandEnd = c;
                    int operandLength = operandEnd - operandStart;

                    // Search previous defines for operand
                    bool foundOperand = false;
                    for (int previousDefineIndex = 0; previousDefineIndex < defineIndex; previousDefineIndex++)
                    {
                        if (IsTextEqual(defines[previousDefineIndex].name, &valuePtr[operandStart], operandLength))
                        {
                            if ((defines[previousDefineIndex].type >= INT) && (defines[previousDefineIndex].type <= DOUBLE_MATH))
                            {
                                // Found operand and it's a number -> update largestType
                                if (defines[previousDefineIndex].type > largestType)
                                    largestType = defines[previousDefineIndex].type;
                                foundOperand = true;
                            }
                            break;
                        }
                    }
                    if (!foundOperand)
                    {
                        isMath = false;
                        break;
                    }
                }
            }

            if (isMath)
            {
                // Define is a calculated number -> update type
                if (largestType == INT)
                    largestType = INT_MATH;
                else if (largestType == LONG)
                    largestType = LONG_MATH;
                else if (largestType == FLOAT)
                    largestType = FLOAT_MATH;
                else if (largestType == DOUBLE)
                    largestType = DOUBLE_MATH;
                defines[defineIndex].type = largestType;
            }
        }

        defineIndex++;
    }
    defineCount = defineIndex;
    free(defineLines);

    // Structs info data
    structs = (StructInfo *)calloc(MAX_STRUCTS_TO_PARSE, sizeof(StructInfo));

    for (int i = 0; i < structCount; i++)
    {
        char **linesPtr = &lines[structLines[i]];

        // Parse struct description
        GetDescription(linesPtr[-1], structs[i].desc);

        // Get struct name: typedef struct name {
        const int TDS_LEN = 15; // length of "typedef struct "
        for (int c = TDS_LEN; c < 64 + TDS_LEN; c++)
        {
            if ((linesPtr[0][c] == '{') || (linesPtr[0][c] == ' '))
            {
                int nameLen = c - TDS_LEN;
                while (linesPtr[0][TDS_LEN + nameLen - 1] == ' ')
                    nameLen--;
                MemoryCopy(structs[i].name, &linesPtr[0][TDS_LEN], nameLen);
                break;
            }
        }

        // Get struct fields and count them -> fields finish with ;
        int l = 1;
        while (linesPtr[l][0] != '}')
        {
            // WARNING: Some structs have empty spaces and comments -> OK, processed
            if ((linesPtr[l][0] != ' ') && (linesPtr[l][0] != '\0'))
            {
                // Scan one field line
                char *fieldLine = linesPtr[l];
                int fieldEndPos = 0;
                while (fieldLine[fieldEndPos] != ';')
                    fieldEndPos++;

                if ((fieldLine[0] != '/') && !IsTextEqual(fieldLine, "struct", 6)) // Field line is not a comment and not a struct declaration
                {
                    // printf("Struct field: %s_\n", fieldLine);     // OK!

                    // Get struct field type and name
                    GetDataTypeAndName(fieldLine, fieldEndPos, structs[i].fieldType[structs[i].fieldCount], structs[i].fieldName[structs[i].fieldCount]);

                    // Get the field description
                    GetDescription(&fieldLine[fieldEndPos], structs[i].fieldDesc[structs[i].fieldCount]);

                    structs[i].fieldCount++;

                    // Split field names containing multiple fields (like Matrix)
                    int additionalFields = 0;
                    int originalIndex = structs[i].fieldCount - 1;
                    for (unsigned int c = 0; c < TextLength(structs[i].fieldName[originalIndex]); c++)
                    {
                        if (structs[i].fieldName[originalIndex][c] == ',')
                            additionalFields++;
                    }

                    if (additionalFields > 0)
                    {
                        int originalLength = -1;
                        int lastStart;
                        for (unsigned int c = 0; c < TextLength(structs[i].fieldName[originalIndex]) + 1; c++)
                        {
                            char v = structs[i].fieldName[originalIndex][c];
                            bool isEndOfString = (v == '\0');
                            if ((v == ',') || isEndOfString)
                            {
                                if (originalLength == -1)
                                {
                                    // Save length of original field name
                                    // Don't truncate yet, still needed for copying
                                    originalLength = c;
                                }
                                else
                                {
                                    // Copy field data from original field
                                    int nameLength = c - lastStart;
                                    MemoryCopy(structs[i].fieldName[structs[i].fieldCount], &structs[i].fieldName[originalIndex][lastStart], nameLength);
                                    MemoryCopy(structs[i].fieldType[structs[i].fieldCount], &structs[i].fieldType[originalIndex][0], TextLength(structs[i].fieldType[originalIndex]));
                                    MemoryCopy(structs[i].fieldDesc[structs[i].fieldCount], &structs[i].fieldDesc[originalIndex][0], TextLength(structs[i].fieldDesc[originalIndex]));
                                    structs[i].fieldCount++;
                                }
                                if (!isEndOfString)
                                {
                                    // Skip comma and spaces
                                    c++;
                                    while (structs[i].fieldName[originalIndex][c] == ' ')
                                        c++;

                                    // Save position for next field
                                    lastStart = c;
                                }
                            }
                        }
                        // Set length of original field to truncate the first field name
                        structs[i].fieldName[originalIndex][originalLength] = '\0';
                    }

                    // Split field types containing multiple fields (like MemNode)
                    additionalFields = 0;
                    originalIndex = structs[i].fieldCount - 1;
                    for (unsigned int c = 0; c < TextLength(structs[i].fieldType[originalIndex]); c++)
                    {
                        if (structs[i].fieldType[originalIndex][c] == ',')
                            additionalFields++;
                    }

                    if (additionalFields > 0)
                    {
                        // Copy original name to last additional field
                        structs[i].fieldCount += additionalFields;
                        MemoryCopy(structs[i].fieldName[originalIndex + additionalFields], &structs[i].fieldName[originalIndex][0], TextLength(structs[i].fieldName[originalIndex]));

                        // Copy names from type to additional fields
                        int fieldsRemaining = additionalFields;
                        int nameStart = -1;
                        int nameEnd = -1;
                        for (int k = TextLength(structs[i].fieldType[originalIndex]); k > 0; k--)
                        {
                            char v = structs[i].fieldType[originalIndex][k];
                            if ((v == '*') || (v == ' ') || (v == ','))
                            {
                                if (nameEnd != -1)
                                {
                                    // Don't copy to last additional field
                                    if (fieldsRemaining != additionalFields)
                                    {
                                        nameStart = k + 1;
                                        MemoryCopy(structs[i].fieldName[originalIndex + fieldsRemaining], &structs[i].fieldType[originalIndex][nameStart], nameEnd - nameStart + 1);
                                    }
                                    nameEnd = -1;
                                    fieldsRemaining--;
                                }
                            }
                            else if (nameEnd == -1)
                                nameEnd = k;
                        }

                        // Truncate original field type
                        int fieldTypeLength = nameStart;
                        structs[i].fieldType[originalIndex][fieldTypeLength] = '\0';

                        // Set field type and description of additional fields
                        for (int j = 1; j <= additionalFields; j++)
                        {
                            MemoryCopy(structs[i].fieldType[originalIndex + j], &structs[i].fieldType[originalIndex][0], fieldTypeLength);
                            MemoryCopy(structs[i].fieldDesc[originalIndex + j], &structs[i].fieldDesc[originalIndex][0], TextLength(structs[i].fieldDesc[originalIndex]));
                        }
                    }
                }
            }

            l++;
        }

        // Move array sizes from name to type
        for (int j = 0; j < structs[i].fieldCount; j++)
        {
            MoveArraySize(structs[i].fieldName[j], structs[i].fieldType[j]);
        }
    }
    free(structLines);

    // Alias info data
    aliases = (AliasInfo *)calloc(MAX_ALIASES_TO_PARSE, sizeof(AliasInfo));

    for (int i = 0; i < aliasCount; i++)
    {
        // Description from previous line
        GetDescription(lines[aliasLines[i] - 1], aliases[i].desc);

        char *linePtr = lines[aliasLines[i]];

        // Skip "typedef "
        int c = 8;

        // Type
        int typeStart = c;
        while (linePtr[c] != ' ')
            c++;
        int typeLen = c - typeStart;
        MemoryCopy(aliases[i].type, &linePtr[typeStart], typeLen);

        // Skip space
        c++;

        // Name
        int nameStart = c;
        while (linePtr[c] != ';')
            c++;
        int nameLen = c - nameStart;
        MemoryCopy(aliases[i].name, &linePtr[nameStart], nameLen);

        // Description
        GetDescription(&linePtr[c], aliases[i].desc);
    }
    free(aliasLines);

    // Enum info data
    enums = (EnumInfo *)calloc(MAX_ENUMS_TO_PARSE, sizeof(EnumInfo));

    for (int i = 0; i < enumCount; i++)
    {
        // Parse enum description
        // NOTE: This is not necessarily from the line immediately before,
        // some of the enums have extra lines between the "description"
        // and the typedef enum
        for (int j = enumLines[i] - 1; j > 0; j--)
        {
            char *linePtr = lines[j];
            if ((linePtr[0] != '/') || (linePtr[2] != ' '))
            {
                GetDescription(&lines[j + 1][0], enums[i].desc);
                break;
            }
        }

        for (int j = 1; j < MAX_ENUM_VALUES * 2; j++) // Maximum number of lines following enum first line
        {
            char *linePtr = lines[enumLines[i] + j];

            if ((linePtr[0] >= 'A') && (linePtr[0] <= 'Z'))
            {
                // Parse enum value line, possible options:
                // ENUM_VALUE_NAME,
                // ENUM_VALUE_NAME
                // ENUM_VALUE_NAME     = 99
                // ENUM_VALUE_NAME     = 99,
                // ENUM_VALUE_NAME     = 0x00000040,   // Value description

                // We start reading the value name
                int c = 0;
                while ((linePtr[c] != ',') &&
                       (linePtr[c] != ' ') &&
                       (linePtr[c] != '=') &&
                       (linePtr[c] != '\0'))
                {
                    enums[i].valueName[enums[i].valueCount][c] = linePtr[c];
                    c++;
                }

                // After the name we can have:
                //  '='  -> value is provided
                //  ','  -> value is equal to previous + 1, there could be a description if not '\0'
                //  ' '  -> value is equal to previous + 1, there could be a description if not '\0'
                //  '\0' -> value is equal to previous + 1

                // Let's start checking if the line is not finished
                if ((linePtr[c] != ',') && (linePtr[c] != '\0'))
                {
                    // Two options:
                    //  '='  -> value is provided
                    //  ' '  -> value is equal to previous + 1, there could be a description if not '\0'
                    bool foundValue = false;
                    while ((linePtr[c] != '\0') && (linePtr[c] != '/'))
                    {
                        if (linePtr[c] == '=')
                        {
                            foundValue = true;
                            break;
                        }
                        c++;
                    }

                    if (foundValue)
                    {
                        if (linePtr[c + 1] == ' ')
                            c += 2;
                        else
                            c++;

                        // Parse integer value
                        int n = 0;
                        char integer[16] = {0};

                        while ((linePtr[c] != ',') && (linePtr[c] != ' ') && (linePtr[c] != '\0'))
                        {
                            integer[n] = linePtr[c];
                            c++;
                            n++;
                        }

                        if (integer[1] == 'x')
                            enums[i].valueInteger[enums[i].valueCount] = (int)strtol(integer, NULL, 16);
                        else
                            enums[i].valueInteger[enums[i].valueCount] = atoi(integer);
                    }
                    else
                        enums[i].valueInteger[enums[i].valueCount] = (enums[i].valueInteger[enums[i].valueCount - 1] + 1);
                }
                else
                    enums[i].valueInteger[enums[i].valueCount] = (enums[i].valueInteger[enums[i].valueCount - 1] + 1);

                // Parse value description
                GetDescription(&linePtr[c], enums[i].valueDesc[enums[i].valueCount]);

                enums[i].valueCount++;
            }
            else if (linePtr[0] == '}')
            {
                // Get enum name from typedef
                int c = 0;
                while (linePtr[2 + c] != ';')
                {
                    enums[i].name[c] = linePtr[2 + c];
                    c++;
                }

                break; // Enum ended, break for() loop
            }
        }
    }
    free(enumLines);

    // Callback info data
    callbacks = (FunctionInfo *)calloc(MAX_CALLBACKS_TO_PARSE, sizeof(FunctionInfo));

    for (int i = 0; i < callbackCount; i++)
    {
        char *linePtr = lines[callbackLines[i]];

        // Skip "typedef "
        unsigned int c = 8;

        // Return type
        int retTypeStart = c;
        while (linePtr[c] != '(')
            c++;
        int retTypeLen = c - retTypeStart;
        while (linePtr[retTypeStart + retTypeLen - 1] == ' ')
            retTypeLen--;
        MemoryCopy(callbacks[i].retType, &linePtr[retTypeStart], retTypeLen);

        // Skip "(*"
        c += 2;

        // Name
        int nameStart = c;
        while (linePtr[c] != ')')
            c++;
        int nameLen = c - nameStart;
        MemoryCopy(callbacks[i].name, &linePtr[nameStart], nameLen);

        // Skip ")("
        c += 2;

        // Params
        int paramStart = c;
        for (; c < MAX_LINE_LENGTH; c++)
        {
            if ((linePtr[c] == ',') || (linePtr[c] == ')'))
            {
                // Get parameter type + name, extract info
                int paramLen = c - paramStart;
                GetDataTypeAndName(&linePtr[paramStart], paramLen, callbacks[i].paramType[callbacks[i].paramCount], callbacks[i].paramName[callbacks[i].paramCount]);
                callbacks[i].paramCount++;
                paramStart = c + 1;
                while (linePtr[paramStart] == ' ')
                    paramStart++;
            }
            if (linePtr[c] == ')')
                break;
        }

        // Description
        GetDescription(&linePtr[c], callbacks[i].desc);

        // Move array sizes from name to type
        for (int j = 0; j < callbacks[i].paramCount; j++)
        {
            MoveArraySize(callbacks[i].paramName[j], callbacks[i].paramType[j]);
        }
    }
    free(callbackLines);

    // Functions info data
    funcs = (FunctionInfo *)calloc(MAX_FUNCS_TO_PARSE, sizeof(FunctionInfo));

    for (int i = 0; i < funcCount; i++)
    {
        char *linePtr = lines[funcLines[i]];

        int funcParamsStart = 0;
        int funcEnd = 0;

        // Get return type and function name from func line
        for (int c = 0; (c < MAX_LINE_LENGTH) && (linePtr[c] != '\n'); c++)
        {
            if (linePtr[c] == '(') // Starts function parameters
            {
                funcParamsStart = c + 1;

                // At this point we have function return type and function name
                char funcRetTypeName[128] = {0};
                int dc = TextLength(apiDefine) + 1;
                int funcRetTypeNameLen = c - dc; // Substract `define` ("RLAPI " for raylib.h)
                MemoryCopy(funcRetTypeName, &linePtr[dc], funcRetTypeNameLen);

                GetDataTypeAndName(funcRetTypeName, funcRetTypeNameLen, funcs[i].retType, funcs[i].name);
                break;
            }
        }

        // Get parameters from func line
        for (int c = funcParamsStart; c < MAX_LINE_LENGTH; c++)
        {
            if (linePtr[c] == ',') // Starts function parameters
            {
                // Get parameter type + name, extract info
                char funcParamTypeName[128] = {0};
                int funcParamTypeNameLen = c - funcParamsStart;
                MemoryCopy(funcParamTypeName, &linePtr[funcParamsStart], funcParamTypeNameLen);

                GetDataTypeAndName(funcParamTypeName, funcParamTypeNameLen, funcs[i].paramType[funcs[i].paramCount], funcs[i].paramName[funcs[i].paramCount]);

                funcParamsStart = c + 1;
                if (linePtr[c + 1] == ' ')
                    funcParamsStart += 1;
                funcs[i].paramCount++; // Move to next parameter
            }
            else if (linePtr[c] == ')')
            {
                funcEnd = c + 2;

                // Check if there are no parameters
                if ((funcEnd - funcParamsStart == 2) ||
                    ((linePtr[c - 4] == 'v') &&
                     (linePtr[c - 3] == 'o') &&
                     (linePtr[c - 2] == 'i') &&
                     (linePtr[c - 1] == 'd')))
                {
                    break;
                }

                // Get parameter type + name, extract info
                char funcParamTypeName[128] = {0};
                int funcParamTypeNameLen = c - funcParamsStart;
                MemoryCopy(funcParamTypeName, &linePtr[funcParamsStart], funcParamTypeNameLen);

                GetDataTypeAndName(funcParamTypeName, funcParamTypeNameLen, funcs[i].paramType[funcs[i].paramCount], funcs[i].paramName[funcs[i].paramCount]);

                funcs[i].paramCount++; // Move to next parameter
                break;
            }
        }

        // Get function description
        GetDescription(&linePtr[funcEnd], funcs[i].desc);

        // Move array sizes from name to type
        for (int j = 0; j < funcs[i].paramCount; j++)
        {
            MoveArraySize(funcs[i].paramName[j], funcs[i].paramType[j]);
        }
    }
    free(funcLines);

    UnloadTextLines(lines, lineCount);

    // At this point, all raylib data has been parsed!
    //----------------------------------------------------------------------------------
    // defines[]   -> We have all the defines decomposed into pieces for further analysis
    // structs[]   -> We have all the structs decomposed into pieces for further analysis
    // aliases[]   -> We have all the aliases decomposed into pieces for further analysis
    // enums[]     -> We have all the enums decomposed into pieces for further analysis
    // callbacks[] -> We have all the callbacks decomposed into pieces for further analysis
    // funcs[]     -> We have all the functions decomposed into pieces for further analysis

    // Export data as required
    // NOTE: We are exporting data in several common formats (JSON, XML, LUA...) for convenience
    // but this data can be directly used to create bindings for other languages modifying this
    // small parser for every binding need, there is no need to use (and re-parse) the
    // generated files... despite it seems that's the usual approach...
    printf("\nInput file:       %s", inFileName);
    printf("\nOutput file:      %s", outFileName);
    if (outputFormat == DEFAULT)
        printf("\nOutput format:    DEFAULT\n\n");
    else if (outputFormat == JSON)
        printf("\nOutput format:    JSON\n\n");
    else if (outputFormat == XML)
        printf("\nOutput format:    XML\n\n");
    else if (outputFormat == LUA)
        printf("\nOutput format:    LUA\n\n");
    else if (outputFormat == CODE)
        printf("\nOutput format:    CODE\n\n");

    ExportParsedData(outFileName, outputFormat);

    free(defines);
    free(structs);
    free(aliases);
    free(enums);
    free(callbacks);
    free(funcs);
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Show command line usage info
static void ShowCommandLineInfo(void)
{
    printf("\n//////////////////////////////////////////////////////////////////////////////////\n");
    printf("//                                                                              //\n");
    printf("// rlparser - raylib header API parser                                          //\n");
    printf("//                                                                              //\n");
    printf("// more info and bugs-report: github.com/raysan5/raylib/tools/rlparser          //\n");
    printf("//                                                                              //\n");
    printf("// Copyright (c) 2021-2026 Ramon Santamaria (@raysan5)                          //\n");
    printf("//                                                                              //\n");
    printf("//////////////////////////////////////////////////////////////////////////////////\n\n");

    printf("USAGE:\n\n");
    printf("    > rlparser [--help] [--input <filename.h>] [--output <filename.ext>] [--format <type>]\n");

    printf("\nOPTIONS:\n\n");
    printf("    -h, --help                      : Show tool version and command line usage help\n\n");
    printf("    -i, --input <filename.h>        : Define input header file to parse.\n");
    printf("                                      NOTE: If not specified, defaults to: raylib.h\n\n");
    printf("    -o, --output <filename.ext>     : Define output file and format.\n");
    printf("                                      Supported extensions: .txt, .json, .xml, .lua, .h\n");
    printf("                                      NOTE: If not specified, defaults to: raylib_api.txt\n\n");
    printf("    -f, --format <type>             : Define output format for parser data.\n");
    printf("                                      Supported types: DEFAULT, JSON, XML, LUA, CODE\n\n");
    printf("    -d, --define <DEF>              : Define functions specifiers (i.e. RLAPI for raylib.h, RMAPI for raymath.h, etc.)\n");
    printf("                                      NOTE: If no specifier defined, defaults to: RLAPI\n\n");
    printf("    -t, --truncate <after>          : Define string to truncate input after (i.e. \"RLGL IMPLEMENTATION\" for rlgl.h)\n");
    printf("                                      NOTE: If not specified, the full input file is parsed.\n\n");

    printf("\nEXAMPLES:\n\n");
    printf("    > rlparser --input raylib.h --output api.json\n");
    printf("        Process <raylib.h> to generate <api.json>\n\n");
    printf("    > rlparser --output raylib_data.info --format XML\n");
    printf("        Process <raylib.h> to generate <raylib_data.info> as XML text data\n\n");
    printf("    > rlparser --input raymath.h --output raymath_data.info --format XML --define RMAPI\n");
    printf("        Process <raymath.h> to generate <raymath_data.info> as XML text data\n\n");
}

// Process command line arguments
static void ProcessCommandLine(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (IsTextEqual(argv[i], "-h", 2) || IsTextEqual(argv[i], "--help", 6))
        {
            // Show info
            ShowCommandLineInfo();
            exit(0);
        }
        else if (IsTextEqual(argv[i], "-i", 2) || IsTextEqual(argv[i], "--input", 7))
        {
            // Check for valid argument and valid file extension
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                MemoryCopy(inFileName, argv[i + 1], TextLength(argv[i + 1])); // Read input filename
                i++;
            }
            else
                printf("WARNING: No input file provided\n");
        }
        else if (IsTextEqual(argv[i], "-o", 2) || IsTextEqual(argv[i], "--output", 8))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                MemoryCopy(outFileName, argv[i + 1], TextLength(argv[i + 1])); // Read output filename
                i++;
            }
            else
                printf("WARNING: No output file provided\n");
        }
        else if (IsTextEqual(argv[i], "-f", 2) || IsTextEqual(argv[i], "--format", 8))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                if (IsTextEqual(argv[i + 1], "DEFAULT\0", 8))
                    outputFormat = DEFAULT;
                else if (IsTextEqual(argv[i + 1], "JSON\0", 5))
                    outputFormat = JSON;
                else if (IsTextEqual(argv[i + 1], "XML\0", 4))
                    outputFormat = XML;
                else if (IsTextEqual(argv[i + 1], "LUA\0", 4))
                    outputFormat = LUA;
                else if (IsTextEqual(argv[i + 1], "CODE\0", 5))
                    outputFormat = CODE;
            }
            else
                printf("WARNING: No format parameters provided\n");
        }
        else if (IsTextEqual(argv[i], "-d", 2) || IsTextEqual(argv[i], "--define", 8))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                MemoryCopy(apiDefine, argv[i + 1], TextLength(argv[i + 1])); // Read functions define
                apiDefine[TextLength(argv[i + 1])] = '\0';
                i++;
            }
            else
                printf("WARNING: No define key provided\n");
        }
        else if (IsTextEqual(argv[i], "-t", 2) || IsTextEqual(argv[i], "--truncate", 10))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                MemoryCopy(truncAfter, argv[i + 1], TextLength(argv[i + 1])); // Read truncate marker
                truncAfter[TextLength(argv[i + 1])] = '\0';
                i++;
            }
        }
    }
}

// Load text data from file, returns a '\0' terminated string
// NOTE: text chars array should be freed manually
static char *LoadFileText(const char *fileName, int *length)
{
    char *text = NULL;

    if (fileName != NULL)
    {
        FILE *file = fopen(fileName, "rt");

        if (file != NULL)
        {
            // WARNING: When reading a file as 'text' file,
            // text mode causes carriage return-linefeed translation...
            // ...but using fseek() should return correct byte-offset
            fseek(file, 0, SEEK_END);
            int size = ftell(file);
            fseek(file, 0, SEEK_SET);

            if (size > 0)
            {
                text = (char *)calloc((size + 1), sizeof(char));
                unsigned int count = (unsigned int)fread(text, sizeof(char), size, file);

                // WARNING: \r\n is converted to \n on reading, so,
                // read bytes count gets reduced by the number of lines
                if (count < (unsigned int)size)
                {
                    text = realloc(text, count + 1);
                    *length = count;
                }
                else
                    *length = size;

                // Zero-terminate the string
                text[count] = '\0';
            }

            fclose(file);
        }
    }

    return text;
}

// Unload text data
static void UnloadFileText(char *text)
{
    free(text);
}

// Get all lines from a text buffer (expecting lines ending with '\n')
static char **LoadTextLines(const char *buffer, int length, int *lineCount)
{
    // Get the number of lines in the text
    int count = 0;
    for (int i = 0; i < length; i++)
        if (buffer[i] == '\n')
            count++;

    printf("Number of text lines in buffer: %i\n", count);

    // Allocate as many pointers as lines
    char **lines = (char **)malloc(count * sizeof(char **));

    char *bufferPtr = (char *)buffer;

    for (int i = 0; (i < count) || (bufferPtr[0] != '\0'); i++)
    {
        lines[i] = (char *)calloc(MAX_LINE_LENGTH, sizeof(char)); // MAX_LINE_LENGTH=1024

        // Remove line leading spaces
        // Find last index of space/tab character
        int index = 0;
        while ((bufferPtr[index] == ' ') || (bufferPtr[index] == '\t'))
            index++;

        int j = 0;
        while (bufferPtr[index + j] != '\n' && bufferPtr[index + j] != '\0')
        {
            lines[i][j] = bufferPtr[index + j];
            j++;
        }

        bufferPtr += (index + j + 1);
    }

    *lineCount = count;
    return lines;
}

// Unload text lines data
static void UnloadTextLines(char **lines, int lineCount)
{
    for (int i = 0; i < lineCount; i++)
        free(lines[i]);
    free(lines);
}

// Get data type and name from a string containing both
// NOTE: Useful to parse function parameters and struct fields
static void GetDataTypeAndName(const char *typeName, int typeNameLen, char *type, char *name)
{
    for (int k = typeNameLen; k > 0; k--)
    {
        if ((typeName[k] == ' ') && (typeName[k - 1] != ','))
        {
            // Function name starts at this point (and ret type finishes at this point)
            MemoryCopy(type, typeName, k);
            MemoryCopy(name, typeName + k + 1, typeNameLen - k - 1);
            break;
        }
        else if (typeName[k] == '*')
        {
            MemoryCopy(type, typeName, k + 1);
            MemoryCopy(name, typeName + k + 1, typeNameLen - k - 1);
            break;
        }
        else if ((typeName[k] == '.') && (typeNameLen == 3)) // Handle varargs ...);
        {
            const char *varargsDots = "...";
            const char *varargsArg = "args";
            MemoryCopy(type, varargsDots, TextLength(varargsDots));
            MemoryCopy(name, varargsArg, TextLength(varargsArg));
            break;
        }
    }
}

// Get comment from a line, do nothing if no comment in line
static void GetDescription(const char *line, char *description)
{
    int c = 0;
    int descStart = -1;
    int lastSlash = -2;
    bool isValid = false;
    while (line[c] != '\0')
    {
        if (isValid && (descStart == -1) && (line[c] != ' '))
            descStart = c;
        else if (line[c] == '/')
        {
            if (lastSlash == c - 1)
                isValid = true;
            lastSlash = c;
        }
        c++;
    }
    if (descStart != -1)
        MemoryCopy(description, &line[descStart], c - descStart);
}

// Move array size from name to type
static void MoveArraySize(char *name, char *type)
{
    int nameLength = TextLength(name);
    if (name[nameLength - 1] == ']')
    {
        for (int k = nameLength; k > 0; k--)
        {
            if (name[k] == '[')
            {
                int sizeLength = nameLength - k;
                MemoryCopy(&type[TextLength(type)], &name[k], sizeLength);
                name[k] = '\0';
            }
        }
    }
}

// Get text length in bytes, check for \0 character
static unsigned int TextLength(const char *text)
{
    unsigned int length = 0;

    if (text != NULL)
        while (*text++)
            length++;

    return length;
}

// Compare two text strings, requires number of characters to compare
static bool IsTextEqual(const char *text1, const char *text2, unsigned int count)
{
    bool result = true;

    for (unsigned int i = 0; i < count; i++)
    {
        if (text1[i] != text2[i])
        {
            result = false;
            break;
        }
    }

    return result;
}

// Find first text occurrence within a string
int TextFindIndex(const char *text, const char *find)
{
    int textLen = TextLength(text);
    int findLen = TextLength(find);

    for (int i = 0; i <= textLen - findLen; i++)
    {
        if (IsTextEqual(&text[i], find, findLen))
            return i;
    }

    return -1;
}

// Custom memcpy() to avoid <string.h>
static void MemoryCopy(void *dest, const void *src, unsigned int count)
{
    char *srcPtr = (char *)src;
    char *destPtr = (char *)dest;

    for (unsigned int i = 0; i < count; i++)
        destPtr[i] = srcPtr[i];
}

// Escape backslashes in a string, writing the escaped string into a static buffer
static char *EscapeBackslashes(char *text)
{
    static char buffer[256] = {0};

    int count = 0;

    for (int i = 0; (text[i] != '\0') && (i < 255); i++, count++)
    {
        buffer[count] = text[i];

        if (text[i] == '\\')
        {
            buffer[count + 1] = '\\';
            count++;
        }
    }

    buffer[count] = '\0';

    return buffer;
}

// Get string of define type
static const char *StrDefineType(DefineType type)
{
    switch (type)
    {
    case UNKNOWN:
        return "UNKNOWN";
    case GUARD:
        return "GUARD";
    case MACRO:
        return "MACRO";
    case INT:
        return "INT";
    case INT_MATH:
        return "INT_MATH";
    case LONG:
        return "LONG";
    case LONG_MATH:
        return "LONG_MATH";
    case FLOAT:
        return "FLOAT";
    case FLOAT_MATH:
        return "FLOAT_MATH";
    case DOUBLE:
        return "DOUBLE";
    case DOUBLE_MATH:
        return "DOUBLE_MATH";
    case CHAR:
        return "CHAR";
    case STRING:
        return "STRING";
    case COLOR:
        return "COLOR";
    }
    return "";
}

/*
// Replace text string
// REQUIRES: strlen(), strstr(), strncpy(), strcpy() -> TODO: Replace by custom implementations!
// WARNING: Returned buffer must be freed by the user (if return != NULL)
static char *TextReplace(char *text, const char *replace, const char *by)
{
    // Sanity checks and initialization
    if (!text || !replace || !by) return NULL;

    char *result;

    char *insertPoint;      // Next insert point
    char *temp;             // Temp pointer
    int replaceLen;         // Replace string length of (the string to remove)
    int byLen;              // Replacement length (the string to replace replace by)
    int lastReplacePos;     // Distance between replace and end of last replace
    int count;              // Number of replacements

    replaceLen = strlen(replace);
    if (replaceLen == 0) return NULL;  // Empty replace causes infinite loop during count

    byLen = strlen(by);

    // Count the number of replacements needed
    insertPoint = text;
    for (count = 0; (temp = strstr(insertPoint, replace)); count++) insertPoint = temp + replaceLen;

    // Allocate returning string and point temp to it
    temp = result = (char *)malloc(strlen(text) + (byLen - replaceLen)*count + 1);

    if (!result) return NULL;   // Memory could not be allocated

    // First time through the loop, all the variable are set correctly from here on,
    //  - 'temp' points to the end of the result string
    //  - 'insertPoint' points to the next occurrence of replace in text
    //  - 'text' points to the remainder of text after "end of replace"
    while (count--)
    {
        insertPoint = strstr(text, replace);
        lastReplacePos = (int)(insertPoint - text);
        temp = strncpy(temp, text, lastReplacePos) + lastReplacePos;
        temp = strcpy(temp, by) + byLen;
        text += lastReplacePos + replaceLen; // Move to next "end of replace"
    }

    // Copy remaind text part after replacement to result (pointed by moving temp)
    strcpy(temp, text);

    return result;
}
*/

static void ExportCodeBundle(const char *rootDir)
{
    EnsureCodegenDirectories(rootDir);

    if (!IsRaymathApi())
    {
        ExportMojoTypes(rootDir);
        ExportMojoPublicTypes(rootDir);
        ExportMojoRawModule(rootDir, "raylib", "raylib");
        ExportMojoRawInit(rootDir);
        ExportMojoSafe(rootDir);
        ExportMojoPackageInit(rootDir);
    }
    else
    {
        ExportMojoRawModule(rootDir, "raymath", "mojo_raymath");
        ExportNativeShim(rootDir);
        ExportMojoRaymathSafe(rootDir);
        ExportMojoRawInit(rootDir);
        ExportMojoPackageInit(rootDir);
    }
}

static void ExportMojoTypes(const char *rootDir)
{
    char fileName[1024] = {0};
    JoinPath(rootDir, "mojo_raylib/raw/types.mojo", fileName, sizeof(fileName));

    FILE *outFile = fopen(fileName, "wt");
    if (outFile == NULL)
        return;

    WriteGeneratedHeader(outFile, "Low-level raylib/raymath type declarations", inFileName);
    WriteMojoCommonImports(outFile);
    fprintf(outFile, "\n");

    // Forward-declared opaque types from raylib.h (e.g. `typedef struct rAudioBuffer rAudioBuffer;`).
    // The parser intentionally skips these (line ~355) but other structs reference them.
    fprintf(outFile, "# Opaque forward-declared types (raylib internals)\n");
    fprintf(outFile, "@fieldwise_init\nstruct rAudioBuffer(Copyable, ImplicitlyCopyable, Movable):\n    var _opaque: c_int\n\n");
    fprintf(outFile, "@fieldwise_init\nstruct rAudioProcessor(Copyable, ImplicitlyCopyable, Movable):\n    var _opaque: c_int\n\n");

    // Aliases first so structs that use them resolve. Mojo doesn't forward-reference comptime aliases.
    for (int i = 0; i < aliasCount; i++)
    {
        char aliasTarget[256] = {0};
        char comment[256] = {0};
        char aliasName[128] = {0};
        int start = 0;

        if (aliases[i].name[0] == '*')
            start = 1;
        CopyText(aliasName, aliases[i].name + start, sizeof(aliasName));
        GetMojoAliasTarget(&aliases[i], aliasTarget, sizeof(aliasTarget));
        SanitizeComment(aliases[i].desc, comment, sizeof(comment));
        if (comment[0] != '\0')
            fprintf(outFile, "# %s\n", comment);
        fprintf(outFile, "comptime %s = %s\n\n", aliasName, aliasTarget);
    }

    for (int i = 0; i < structCount; i++)
    {
        char comment[256] = {0};
        SanitizeComment(structs[i].desc, comment, sizeof(comment));
        if (comment[0] != '\0')
            fprintf(outFile, "# %s\n", comment);

        // Pre-scan: if any field is an array of a known struct (e.g. Matrix[2]),
        // we can't be RegisterPassable because InlineArray of struct isn't.
        bool canBeRP = true;
        for (int j = 0; j < structs[i].fieldCount; j++)
        {
            int arrAt = TextFindIndex(structs[i].fieldType[j], "[");
            if (arrAt > -1)
            {
                char arrBase[128] = {0};
                for (int k = 0; k < arrAt; k++)
                    arrBase[k] = structs[i].fieldType[j][k];
                arrBase[arrAt] = '\0';
                char trimmedAB[128] = {0};
                CopyTrimmed(trimmedAB, arrBase, sizeof(trimmedAB));
                if (IsKnownStructOrAlias(trimmedAB))
                {
                    canBeRP = false;
                    break;
                }
            }
        }

        fprintf(outFile, "@fieldwise_init\n");
        if (canBeRP)
            fprintf(outFile, "struct %s(TrivialRegisterPassable):\n", structs[i].name);
        else
            fprintf(outFile, "struct %s(Copyable, ImplicitlyCopyable, Movable):\n", structs[i].name);

        if (structs[i].fieldCount == 0)
            fprintf(outFile, "    var _unused: c_int\n");
        for (int j = 0; j < structs[i].fieldCount; j++)
        {
            char mojoType[256] = {0};
            char fieldComment[256] = {0};
            GetMojoType(structs[i].fieldType[j], false, false, mojoType, sizeof(mojoType));
            SanitizeComment(structs[i].fieldDesc[j], fieldComment, sizeof(fieldComment));

            if (fieldComment[0] != '\0')
                fprintf(outFile, "    # %s\n", fieldComment);
            fprintf(outFile, "    var %s: %s\n", structs[i].fieldName[j], mojoType);
        }

        fprintf(outFile, "\n");
    }

    for (int i = 0; i < enumCount; i++)
    {
        char comment[256] = {0};
        SanitizeComment(enums[i].desc, comment, sizeof(comment));
        if (comment[0] != '\0')
            fprintf(outFile, "# %s\n", comment);
        fprintf(outFile, "comptime %s = c_int\n", enums[i].name);
        for (int j = 0; j < enums[i].valueCount; j++)
        {
            char valueComment[256] = {0};
            SanitizeComment(enums[i].valueDesc[j], valueComment, sizeof(valueComment));
            if (valueComment[0] != '\0')
                fprintf(outFile, "# %s\n", valueComment);
            fprintf(outFile, "comptime %s = %i\n", enums[i].valueName[j], enums[i].valueInteger[j]);
        }
        fprintf(outFile, "\n");
    }

    for (int i = 0; i < callbackCount; i++)
    {
        char returnType[256] = {0};
        char comment[256] = {0};
        GetMojoType(callbacks[i].retType, true, false, returnType, sizeof(returnType));
        SanitizeComment(callbacks[i].desc, comment, sizeof(comment));

        if (comment[0] != '\0')
            fprintf(outFile, "# %s\n", comment);
        fprintf(outFile, "comptime %s = fn(", callbacks[i].name);
        WriteMojoSignatureParams(outFile, &callbacks[i], true);
        fprintf(outFile, ") -> %s\n\n", returnType);
    }

    for (int i = 0; i < defineCount; i++)
    {
        if ((defines[i].type == INT) ||
            (defines[i].type == INT_MATH) ||
            (defines[i].type == LONG) ||
            (defines[i].type == LONG_MATH))
        {
            fprintf(outFile, "comptime %s = %s\n", defines[i].name, defines[i].value);
        }
        else if ((defines[i].type == FLOAT) || (defines[i].type == FLOAT_MATH))
        {
            // Strip C float suffixes (`1.0f`, `2.5F`) — Mojo doesn't accept them.
            char scrubbed[256] = {0};
            int sj = 0;
            for (int si = 0; defines[i].value[si] != '\0' && sj < (int)sizeof(scrubbed) - 1; si++)
            {
                char ch = defines[i].value[si];
                if (((ch == 'f') || (ch == 'F')) && (si > 0))
                {
                    char prev = defines[i].value[si - 1];
                    char next = defines[i].value[si + 1];
                    bool prevIsNum = ((prev >= '0') && (prev <= '9')) || (prev == '.');
                    bool nextIsAlnum = ((next >= 'a') && (next <= 'z')) ||
                                       ((next >= 'A') && (next <= 'Z')) ||
                                       ((next >= '0') && (next <= '9')) ||
                                       (next == '_');
                    if (prevIsNum && !nextIsAlnum)
                        continue;
                }
                scrubbed[sj++] = ch;
            }
            scrubbed[sj] = '\0';
            fprintf(outFile, "comptime %s = %s\n", defines[i].name, scrubbed);
        }
    }

    fclose(outFile);
}

static void ExportMojoPublicTypes(const char *rootDir)
{
    char fileName[1024] = {0};
    JoinPath(rootDir, "mojo_raylib/types.mojo", fileName, sizeof(fileName));

    FILE *outFile = fopen(fileName, "wt");
    if (outFile == NULL)
        return;

    WriteGeneratedHeader(outFile, "Public Mojo value types", inFileName);
    fprintf(outFile, "import mojo_raylib.raw.types as raw_types\n");
    fprintf(outFile, "from std.ffi import c_char, c_uchar, c_short, c_ushort, c_int, c_uint, c_long, c_ulong, c_float, c_double\n");
    fprintf(outFile, "from std.collections import InlineArray\n");
    fprintf(outFile, "from std.memory.unsafe_pointer import UnsafePointer\n\n");

    // Forward-declared opaque types — same stubs as raw/types.mojo for layout parity.
    fprintf(outFile, "@fieldwise_init\nstruct rAudioBuffer(TrivialRegisterPassable):\n    var _opaque: c_int\n\n");
    fprintf(outFile, "@fieldwise_init\nstruct rAudioProcessor(TrivialRegisterPassable):\n    var _opaque: c_int\n\n");

    // Pointer aliases (e.g. ModelAnimPose) emitted up front so structs that use them resolve.
    for (int i = 0; i < aliasCount; i++)
    {
        char aliasTarget[256] = {0};
        char aliasName[128] = {0};
        int start = 0;
        if (aliases[i].name[0] == '*')
            start = 1;
        CopyText(aliasName, aliases[i].name + start, sizeof(aliasName));
        GetMojoAliasTarget(&aliases[i], aliasTarget, sizeof(aliasTarget));
        if (aliases[i].name[0] == '*')
            fprintf(outFile, "comptime %s = %s\n\n", aliasName, aliasTarget);
    }

    for (int i = 0; i < structCount; i++)
    {
        char comment[256] = {0};
        SanitizeComment(structs[i].desc, comment, sizeof(comment));
        if (comment[0] != '\0')
            fprintf(outFile, "# %s\n", comment);

        bool canBeRP = true;
        for (int j = 0; j < structs[i].fieldCount; j++)
        {
            int arrAt = TextFindIndex(structs[i].fieldType[j], "[");
            if (arrAt > -1)
            {
                char arrBase[128] = {0};
                for (int k = 0; k < arrAt; k++)
                    arrBase[k] = structs[i].fieldType[j][k];
                arrBase[arrAt] = '\0';
                char trimmedAB[128] = {0};
                CopyTrimmed(trimmedAB, arrBase, sizeof(trimmedAB));
                if (IsKnownStructOrAlias(trimmedAB))
                {
                    canBeRP = false;
                    break;
                }
            }
        }

        if (canBeRP)
            fprintf(outFile, "struct %s(TrivialRegisterPassable):\n", structs[i].name);
        else
            fprintf(outFile, "struct %s(Copyable, ImplicitlyCopyable, Movable):\n", structs[i].name);
        if (structs[i].fieldCount == 0)
        {
            fprintf(outFile, "    var _unused: Int32\n");
        }
        else
        {
            for (int j = 0; j < structs[i].fieldCount; j++)
            {
                char fieldType[256] = {0};
                char fieldComment[256] = {0};
                GetMojoLayoutType(structs[i].fieldType[j], fieldType, sizeof(fieldType));
                SanitizeComment(structs[i].fieldDesc[j], fieldComment, sizeof(fieldComment));
                if (fieldComment[0] != '\0')
                    fprintf(outFile, "    # %s\n", fieldComment);
                fprintf(outFile, "    var %s: %s\n", structs[i].fieldName[j], fieldType);
            }
        }
        fprintf(outFile, "\n");
        fprintf(outFile, "    def __init__(out self");
        for (int j = 0; j < structs[i].fieldCount; j++)
        {
            char fieldType[256] = {0};
            GetMojoLayoutType(structs[i].fieldType[j], fieldType, sizeof(fieldType));
            fprintf(outFile, ", %s: %s", structs[i].fieldName[j], fieldType);
        }
        fprintf(outFile, "):\n");
        if (structs[i].fieldCount == 0)
            fprintf(outFile, "        self._unused = 0\n");
        else
            for (int j = 0; j < structs[i].fieldCount; j++)
                fprintf(outFile, "        self.%s = %s\n", structs[i].fieldName[j], structs[i].fieldName[j]);
        fprintf(outFile, "\n");

        // For non-RP structs (those with InlineArray of struct), the public/raw
        // layouts are identical so an UnsafePointer reinterpret bitcast is safer
        // than a fieldwise reconstruction (which would need element-by-element
        // conversion of the InlineArray).
        fprintf(outFile, "    @staticmethod\n");
        fprintf(outFile, "    def from_raw(value: raw_types.%s) -> Self:\n", structs[i].name);
        if (!canBeRP)
            fprintf(outFile, "        return UnsafePointer(to=value).bitcast[Self]()[]\n\n");
        else if (structs[i].fieldCount == 0)
            fprintf(outFile, "        return %s(0)\n\n", structs[i].name);
        else
        {
            fprintf(outFile, "        return %s(", structs[i].name);
            for (int j = 0; j < structs[i].fieldCount; j++)
            {
                char rawFieldExpr[256] = {0};
                if (j > 0)
                    fprintf(outFile, ", ");
                snprintf(rawFieldExpr, sizeof(rawFieldExpr), "value.%s", structs[i].fieldName[j]);
                WriteRawTypeToPublicExpr(outFile, structs[i].fieldType[j], rawFieldExpr);
            }
            fprintf(outFile, ")\n\n");
        }

        fprintf(outFile, "    def to_raw(self) -> raw_types.%s:\n", structs[i].name);
        if (!canBeRP)
            fprintf(outFile, "        return UnsafePointer(to=self).bitcast[raw_types.%s]()[]\n\n", structs[i].name);
        else if (structs[i].fieldCount == 0)
            fprintf(outFile, "        return raw_types.%s(c_int(0))\n\n", structs[i].name);
        else
        {
            fprintf(outFile, "        return raw_types.%s(", structs[i].name);
            for (int j = 0; j < structs[i].fieldCount; j++)
            {
                char publicFieldExpr[256] = {0};
                if (j > 0)
                    fprintf(outFile, ", ");
                snprintf(publicFieldExpr, sizeof(publicFieldExpr), "self.%s", structs[i].fieldName[j]);
                WritePublicTypeToRawExpr(outFile, structs[i].fieldType[j], publicFieldExpr);
            }
            fprintf(outFile, ")\n\n");
        }

        {
            char snakeName[128] = {0};
            ToSnakeCase(structs[i].name, snakeName, sizeof(snakeName));
            fprintf(outFile, "@always_inline\n");
            fprintf(outFile, "def _to_raw_%s(value: %s) -> raw_types.%s:\n", snakeName, structs[i].name, structs[i].name);
            fprintf(outFile, "    return value.to_raw()\n\n");
            fprintf(outFile, "@always_inline\n");
            fprintf(outFile, "def _from_raw_%s(value: raw_types.%s) -> %s:\n", snakeName, structs[i].name, structs[i].name);
            fprintf(outFile, "    return %s.from_raw(value)\n\n", structs[i].name);
        }
    }

    for (int i = 0; i < aliasCount; i++)
    {
        char aliasTarget[256] = {0};
        char comment[256] = {0};
        char aliasName[128] = {0};
        char resolvedName[128] = {0};
        char snakeName[128] = {0};
        int start = 0;

        if (aliases[i].name[0] == '*')
            continue;
        CopyText(aliasName, aliases[i].name + start, sizeof(aliasName));
        ResolveStructOrAliasName(aliasName, resolvedName, sizeof(resolvedName));
        GetMojoLayoutType(aliases[i].type, aliasTarget, sizeof(aliasTarget));
        SanitizeComment(aliases[i].desc, comment, sizeof(comment));
        if (comment[0] != '\0')
            fprintf(outFile, "# %s\n", comment);
        fprintf(outFile, "comptime %s = %s\n\n", aliasName, aliasTarget);
        if (IsKnownStructOrAlias(resolvedName))
        {
            ToSnakeCase(aliasName, snakeName, sizeof(snakeName));
            {
                char resolvedSnakeName[128] = {0};
                ToSnakeCase(resolvedName, resolvedSnakeName, sizeof(resolvedSnakeName));
                fprintf(outFile, "@always_inline\n");
                fprintf(outFile, "def _to_raw_%s(value: %s) -> raw_types.%s:\n", snakeName, aliasName, aliasName);
                fprintf(outFile, "    return _to_raw_%s(value)\n\n", resolvedSnakeName);
                fprintf(outFile, "@always_inline\n");
                fprintf(outFile, "def _from_raw_%s(value: raw_types.%s) -> %s:\n", snakeName, aliasName, aliasName);
                fprintf(outFile, "    return _from_raw_%s(value)\n\n", resolvedSnakeName);
            }
        }
    }

    fclose(outFile);
}

static void ExportMojoRawModule(const char *rootDir, const char *moduleName, const char *prefix)
{
    char leaf[256] = {0};
    char fileName[1024] = {0};
    snprintf(leaf, sizeof(leaf), "mojo_raylib/raw/%s.mojo", moduleName);
    JoinPath(rootDir, leaf, fileName, sizeof(fileName));

    FILE *outFile = fopen(fileName, "wt");
    if (outFile == NULL)
        return;

    WriteGeneratedHeader(outFile, moduleName, inFileName);
    fprintf(outFile, "from .types import *\n");
    WriteMojoCommonImports(outFile);
    fprintf(outFile, "\n");

    if (IsRaymathApi())
    {
        fprintf(outFile, "comptime TraceLogCallbackSimple = fn(log_level: c_int, text: UnsafePointer[c_char, MutAnyOrigin]) -> NoneType\n\n");
    }

    for (int i = 0; i < funcCount; i++)
    {
        char returnType[256] = {0};
        char symbolName[128] = {0};

        if (IsUnsupportedFunction(&funcs[i]))
            continue;

        if (IsRaymathApi())
            snprintf(symbolName, sizeof(symbolName), "%s_%s", prefix, funcs[i].name);
        else
            CopyText(symbolName, funcs[i].name, sizeof(symbolName));

        GetMojoType(funcs[i].retType, true, false, returnType, sizeof(returnType));
        fprintf(outFile, "def %s(", funcs[i].name);
        WriteMojoSignatureParams(outFile, &funcs[i], false);
        fprintf(outFile, ")");
        if (!IsVoidType(funcs[i].retType))
            fprintf(outFile, " -> %s", returnType);
        fprintf(outFile, ":\n");
        WriteMojoDocstring(outFile, funcs[i].desc, 4);
        fprintf(outFile, "    ");
        if (!IsVoidType(funcs[i].retType))
            fprintf(outFile, "return ");
        fprintf(outFile, "external_call[\"%s\", %s](", symbolName, returnType);
        WriteMojoCallArgs(outFile, &funcs[i]);
        fprintf(outFile, ")\n\n");
    }

    if (!IsRaymathApi())
    {
        fprintf(outFile, "# Shim-backed helpers for unsupported varargs and callback adaptation.\n");
        fprintf(outFile, "def TraceLogText(log_level: c_int, text: UnsafePointer[c_char, MutAnyOrigin]):\n");
        fprintf(outFile, "    external_call[\"mojo_raylib_TraceLogLiteral\", NoneType](log_level, text)\n\n");
        fprintf(outFile, "def TextFormatText(text: UnsafePointer[c_char, MutAnyOrigin]) -> UnsafePointer[c_char, MutAnyOrigin]:\n");
        fprintf(outFile, "    return external_call[\"mojo_raylib_TextFormatLiteral\", UnsafePointer[c_char, MutAnyOrigin]](text)\n\n");
        fprintf(outFile, "comptime TraceLogCallbackSimple = fn(log_level: c_int, text: UnsafePointer[c_char, MutAnyOrigin]) -> NoneType\n");
        fprintf(outFile, "def SetTraceLogCallbackSimple(callback: TraceLogCallbackSimple):\n");
        fprintf(outFile, "    external_call[\"mojo_raylib_SetTraceLogCallback\", NoneType](callback)\n\n");
    }

    fclose(outFile);
}

static void ExportMojoRawInit(const char *rootDir)
{
    char fileName[1024] = {0};
    JoinPath(rootDir, "mojo_raylib/raw/__init__.mojo", fileName, sizeof(fileName));

    FILE *outFile = fopen(fileName, "wt");
    if (outFile == NULL)
        return;

    WriteGeneratedHeader(outFile, "Raw package exports", inFileName);
    fprintf(outFile, "from .types import *\n");
    fprintf(outFile, "from .raylib import *\n");
    fprintf(outFile, "from .raymath import *\n");
    fclose(outFile);
}

static void ExportMojoSafe(const char *rootDir)
{
    char fileName[1024] = {0};
    JoinPath(rootDir, "mojo_raylib/safe.mojo", fileName, sizeof(fileName));

    FILE *outFile = fopen(fileName, "wt");
    if (outFile == NULL)
        return;

    WriteGeneratedHeader(outFile, "Generated Mojo-native wrappers for raylib", inFileName);
    fprintf(outFile, "from .types import *\n");
    fprintf(outFile, "import mojo_raylib.types as public_types\n");
    fprintf(outFile, "import mojo_raylib.raw.types as raw_types\n");
    fprintf(outFile, "import mojo_raylib.raw.raylib as raw\n");
    fprintf(outFile, "from std.ffi import CStringSlice, c_char, c_uchar, c_int, c_uint, c_float\n");
    fprintf(outFile, "from std.memory import Span\n");
    fprintf(outFile, "from std.memory.unsafe_pointer import UnsafePointer\n\n");

    for (int i = 0; i < funcCount; i++)
    {
        char ownedStructName[128] = {0};
        char releaseName[128] = {0};
        char returnType[256] = {0};
        int releaseMode = 0;
        bool hasOwnedReturn = false;
        bool hasOutCount = false;

        if (IsUnsupportedFunction(&funcs[i]))
            continue;

        releaseMode = FindMatchingReleaseFunction(&funcs[i], releaseName, sizeof(releaseName));
        hasOwnedReturn = (releaseMode > 0);
        if (!hasOwnedReturn)
            continue;

        GetOwnedStructName(&funcs[i], ownedStructName, sizeof(ownedStructName));
        GetMojoType(funcs[i].retType, true, false, returnType, sizeof(returnType));

        // The data field holds a raw FFI pointer; if the pointee is a known struct,
        // qualify it with `raw_types.` so safe.mojo (which imports public types
        // unqualified) resolves to the right struct.
        char ownedDataType[256] = {0};
        {
            char retBaseType[128] = {0};
            bool retIsConst = false;
            int retPointerCount = 0;
            if (GetPointerBaseType(funcs[i].retType, retBaseType, sizeof(retBaseType), &retIsConst, &retPointerCount) &&
                IsKnownStructOrAlias(retBaseType))
            {
                snprintf(ownedDataType, sizeof(ownedDataType),
                    "UnsafePointer[raw_types.%s, MutAnyOrigin]", retBaseType);
            }
            else
            {
                CopyText(ownedDataType, returnType, sizeof(ownedDataType));
            }
        }

        for (int p = 0; p < funcs[i].paramCount; p++)
        {
            if (IsMutablePointerType(funcs[i].paramType[p]) &&
                ((TextFindIndex(funcs[i].paramType[p], "int") > -1) || (TextFindIndex(funcs[i].paramType[p], "unsigned int") > -1)))
            {
                hasOutCount = true;
            }
        }

        fprintf(outFile, "@fieldwise_init\n");
        fprintf(outFile, "struct %s:\n", ownedStructName);
        fprintf(outFile, "    var data: %s\n", ownedDataType);
        if (hasOutCount)
            fprintf(outFile, "    var count: Int\n");
        fprintf(outFile, "    def __del__(deinit self):\n");
        if (releaseMode == 1)
        {
            // Some Unload* helpers want (data, count); detect by inspecting the
            // matching release function's parameter shape.
            int releaseIdx = FindFunctionIndexByName(releaseName);
            bool releaseHasCount = false;
            if (releaseIdx >= 0)
            {
                const FunctionInfo *rel = &funcs[releaseIdx];
                for (int rp = 0; rp < rel->paramCount; rp++)
                {
                    if (IsIntegralCountType(rel->paramType[rp]) && IsCountLikeName(rel->paramName[rp]))
                    {
                        releaseHasCount = true;
                        break;
                    }
                }
            }
            if (releaseHasCount && hasOutCount)
                fprintf(outFile, "        raw.%s(self.data, c_int(self.count))\n\n", releaseName);
            else
                fprintf(outFile, "        raw.%s(self.data)\n\n", releaseName);
        }
        else
            // MemFree expects UnsafePointer[NoneType, MutAnyOrigin]; bitcast to be sure.
            fprintf(outFile, "        raw.%s(self.data.bitcast[NoneType]())\n\n", releaseName);
    }

    for (int i = 0; i < funcCount; i++)
    {
        char returnType[256] = {0};
        char ownedStructName[128] = {0};
        char releaseName[128] = {0};
        char publicFuncName[128] = {0};
        int releaseMode = 0;
        bool returnsVoid = false;
        bool hasOwnedReturn = false;
        int outCountParam = -1;

        if (IsUnsupportedFunction(&funcs[i]))
            continue;

        GetMojoPublicType(funcs[i].retType, true, false, returnType, sizeof(returnType));
        returnsVoid = IsVoidType(funcs[i].retType);
        releaseMode = FindMatchingReleaseFunction(&funcs[i], releaseName, sizeof(releaseName));
        hasOwnedReturn = (releaseMode > 0);
        if (hasOwnedReturn)
            GetOwnedStructName(&funcs[i], ownedStructName, sizeof(ownedStructName));
        ToSnakeCase(funcs[i].name, publicFuncName, sizeof(publicFuncName));

        for (int p = 0; p < funcs[i].paramCount; p++)
        {
            if (IsMutablePointerType(funcs[i].paramType[p]) &&
                ((TextFindIndex(funcs[i].paramType[p], "int") > -1) || (TextFindIndex(funcs[i].paramType[p], "unsigned int") > -1)))
            {
                outCountParam = p;
                break;
            }
        }

        fprintf(outFile, "@always_inline\n");
        fprintf(outFile, "def %s(", publicFuncName);
        bool first = true;
        for (int p = 0; p < funcs[i].paramCount; p++)
        {
            char mojoType[256] = {0};
            char pointeeType[128] = {0};
            char publicParamName[64] = {0};
            int spanCountIndex = -1;
            if (p == outCountParam)
                continue;
            if ((p > 0) && GetSpanCompanionIndex(&funcs[i], p - 1, &spanCountIndex) && (spanCountIndex == p))
                continue;
            if (!first)
                fprintf(outFile, ", ");
            if (GetSpanCompanionIndex(&funcs[i], p, &spanCountIndex))
            {
                GetMojoPublicSpanType(funcs[i].paramType[p], mojoType, sizeof(mojoType));
            }
            else
                GetMojoPublicType(funcs[i].paramType[p], false, false, mojoType, sizeof(mojoType));
            ToSnakeCase(funcs[i].paramName[p], publicParamName, sizeof(publicParamName));
            if (ShouldUseMutValueParam(&funcs[i], p, pointeeType, sizeof(pointeeType)))
            {
                char valueType[256] = {0};
                GetMojoPublicType(pointeeType, false, false, valueType, sizeof(valueType));
                fprintf(outFile, "mut %s: %s", publicParamName, valueType);
            }
            else if (ShouldUseRefValueParam(&funcs[i], p, pointeeType, sizeof(pointeeType)))
            {
                char valueType[256] = {0};
                GetMojoPublicType(pointeeType, false, false, valueType, sizeof(valueType));
                fprintf(outFile, "ref %s: %s", publicParamName, valueType);
            }
            else
                fprintf(outFile, "%s: %s", publicParamName, mojoType);
            first = false;
        }
        fprintf(outFile, ")");
        if (hasOwnedReturn)
            fprintf(outFile, " -> %s", ownedStructName);
        else if (!returnsVoid)
            fprintf(outFile, " -> %s", returnType);
        fprintf(outFile, ":\n");
        WriteMojoDocstring(outFile, funcs[i].desc, 4);

        if (outCountParam > -1)
            fprintf(outFile, "    var count: c_int = 0\n");
        if (!hasOwnedReturn && !returnsVoid)
            fprintf(outFile, "    var result = ");
        else
        {
            fprintf(outFile, "    ");
            if (hasOwnedReturn)
                fprintf(outFile, "var _owned = ");
        }
        fprintf(outFile, "raw.%s(", funcs[i].name);
        first = true;
        for (int p = 0; p < funcs[i].paramCount; p++)
        {
            int spanCountIndex = -1;
            int previousSpanCountIndex = -1;
            char publicParamName[64] = {0};
            if (!first)
                fprintf(outFile, ", ");
            ToSnakeCase(funcs[i].paramName[p], publicParamName, sizeof(publicParamName));
            if (p == outCountParam)
                fprintf(outFile, "UnsafePointer(to=count)");
            else if (GetSpanCompanionIndex(&funcs[i], p, &spanCountIndex))
                WriteSpanPointerToRawExpr(outFile, funcs[i].paramType[p], publicParamName);
            else if ((p > 0) && GetSpanCompanionIndex(&funcs[i], p - 1, &previousSpanCountIndex) && (previousSpanCountIndex == p))
            {
                char previousPublicParamName[64] = {0};
                ToSnakeCase(funcs[i].paramName[p - 1], previousPublicParamName, sizeof(previousPublicParamName));
                fprintf(outFile, "c_int(len(%s))", previousPublicParamName);
            }
            else
                WritePublicToRawArg(outFile, funcs[i].paramType[p], publicParamName);
            first = false;
        }
        fprintf(outFile, ")\n");

        if (hasOwnedReturn)
        {
            if (outCountParam > -1)
                fprintf(outFile, "    return %s(_owned, Int(count))\n\n", ownedStructName);
            else
                fprintf(outFile, "    return %s(_owned)\n\n", ownedStructName);
        }
        else if (returnsVoid)
            fprintf(outFile, "\n");
        else
        {
            fprintf(outFile, "    return ");
            WriteRawToPublicExpr(outFile, funcs[i].retType, "result");
            fprintf(outFile, "\n\n");
        }
    }

    fclose(outFile);
}

static void ExportMojoRaymathSafe(const char *rootDir)
{
    char fileName[1024] = {0};
    JoinPath(rootDir, "mojo_raylib/raymath_safe.mojo", fileName, sizeof(fileName));

    FILE *outFile = fopen(fileName, "wt");
    if (outFile == NULL)
        return;

    WriteGeneratedHeader(outFile, "Generated Mojo-native wrappers for raymath", inFileName);
    fprintf(outFile, "from .types import *\n");
    fprintf(outFile, "import mojo_raylib.types as public_types\n");
    fprintf(outFile, "import mojo_raylib.raw.types as raw_types\n");
    fprintf(outFile, "import mojo_raylib.raw.raymath as raw\n");
    fprintf(outFile, "from std.ffi import c_int, c_uint, c_float\n");
    fprintf(outFile, "from std.memory import Span\n\n");

    for (int i = 0; i < funcCount; i++)
    {
        char returnType[256] = {0};
        char publicFuncName[128] = {0};

        if (IsUnsupportedFunction(&funcs[i]))
            continue;

        GetMojoPublicType(funcs[i].retType, true, false, returnType, sizeof(returnType));
        ToSnakeCase(funcs[i].name, publicFuncName, sizeof(publicFuncName));
        fprintf(outFile, "@always_inline\n");
        fprintf(outFile, "def %s(", publicFuncName);
        bool first = true;
        for (int p = 0; p < funcs[i].paramCount; p++)
        {
            char mojoType[256] = {0};
            char pointeeType[128] = {0};
            char publicParamName[64] = {0};
            int spanCountIndex = -1;
            if ((p > 0) && GetSpanCompanionIndex(&funcs[i], p - 1, &spanCountIndex) && (spanCountIndex == p))
                continue;
            if (!first)
                fprintf(outFile, ", ");
            if (GetSpanCompanionIndex(&funcs[i], p, &spanCountIndex))
                GetMojoPublicSpanType(funcs[i].paramType[p], mojoType, sizeof(mojoType));
            else
                GetMojoPublicType(funcs[i].paramType[p], false, false, mojoType, sizeof(mojoType));
            ToSnakeCase(funcs[i].paramName[p], publicParamName, sizeof(publicParamName));
            if (ShouldUseMutValueParam(&funcs[i], p, pointeeType, sizeof(pointeeType)))
            {
                char valueType[256] = {0};
                GetMojoPublicType(pointeeType, false, false, valueType, sizeof(valueType));
                fprintf(outFile, "mut %s: %s", publicParamName, valueType);
            }
            else if (ShouldUseRefValueParam(&funcs[i], p, pointeeType, sizeof(pointeeType)))
            {
                char valueType[256] = {0};
                GetMojoPublicType(pointeeType, false, false, valueType, sizeof(valueType));
                fprintf(outFile, "ref %s: %s", publicParamName, valueType);
            }
            else
                fprintf(outFile, "%s: %s", publicParamName, mojoType);
            first = false;
        }
        fprintf(outFile, ")");
        if (!IsVoidType(funcs[i].retType))
            fprintf(outFile, " -> %s", returnType);
        fprintf(outFile, ":\n");
        WriteMojoDocstring(outFile, funcs[i].desc, 4);
        if (!IsVoidType(funcs[i].retType))
            fprintf(outFile, "    var result = ");
        else
            fprintf(outFile, "    ");
        fprintf(outFile, "raw.%s(", funcs[i].name);
        for (int p = 0; p < funcs[i].paramCount; p++)
        {
            int spanCountIndex = -1;
            int previousSpanCountIndex = -1;
            char publicParamName[64] = {0};
            if (p > 0)
                fprintf(outFile, ", ");
            ToSnakeCase(funcs[i].paramName[p], publicParamName, sizeof(publicParamName));
            if (GetSpanCompanionIndex(&funcs[i], p, &spanCountIndex))
                WriteSpanPointerToRawExpr(outFile, funcs[i].paramType[p], publicParamName);
            else if ((p > 0) && GetSpanCompanionIndex(&funcs[i], p - 1, &previousSpanCountIndex) && (previousSpanCountIndex == p))
            {
                char previousPublicParamName[64] = {0};
                ToSnakeCase(funcs[i].paramName[p - 1], previousPublicParamName, sizeof(previousPublicParamName));
                fprintf(outFile, "c_int(len(%s))", previousPublicParamName);
            }
            else
                WritePublicToRawArg(outFile, funcs[i].paramType[p], publicParamName);
        }
        fprintf(outFile, ")\n\n");
        if (!IsVoidType(funcs[i].retType))
        {
            fprintf(outFile, "    return ");
            WriteRawToPublicExpr(outFile, funcs[i].retType, "result");
            fprintf(outFile, "\n\n");
        }
    }

    fclose(outFile);
}

static void ExportMojoPackageInit(const char *rootDir)
{
    char fileName[1024] = {0};
    JoinPath(rootDir, "mojo_raylib/__init__.mojo", fileName, sizeof(fileName));

    FILE *outFile = fopen(fileName, "wt");
    if (outFile == NULL)
        return;

    WriteGeneratedHeader(outFile, "Public package exports", inFileName);
    fprintf(outFile, "from .types import *\n");
    fprintf(outFile, "from .safe import *\n");
    fprintf(outFile, "from .raymath_safe import *\n");
    fclose(outFile);
}

static void ExportNativeShim(const char *rootDir)
{
    char fileName[1024] = {0};
    JoinPath(rootDir, "native/mojo_raylib_shim.c", fileName, sizeof(fileName));

    FILE *outFile = fopen(fileName, "wt");
    if (outFile == NULL)
        return;

    fprintf(outFile, "/* Auto-generated by rlparser CODE mode. */\n");
    fprintf(outFile, "#include <stdarg.h>\n");
    fprintf(outFile, "#include <stdio.h>\n");
    fprintf(outFile, "#include <stdbool.h>\n");
    fprintf(outFile, "#include \"../vendor/raylib/src/raylib.h\"\n");
    fprintf(outFile, "#define RAYMATH_STATIC_INLINE\n");
    fprintf(outFile, "#include \"../vendor/raylib/src/raymath.h\"\n\n");
    fprintf(outFile, "#if defined(_WIN32)\n");
    fprintf(outFile, "#define MOJO_RAYLIB_EXPORT __declspec(dllexport)\n");
    fprintf(outFile, "#else\n");
    fprintf(outFile, "#define MOJO_RAYLIB_EXPORT __attribute__((visibility(\"default\")))\n");
    fprintf(outFile, "#endif\n\n");

    fprintf(outFile, "typedef void (*mojo_trace_log_callback_simple)(int logLevel, const char *text);\n");
    fprintf(outFile, "static mojo_trace_log_callback_simple g_mojo_trace_log_callback = NULL;\n\n");
    fprintf(outFile, "static void mojo_trace_log_bridge(int logLevel, const char *text, va_list args)\n");
    fprintf(outFile, "{\n");
    fprintf(outFile, "    if (g_mojo_trace_log_callback == NULL) return;\n");
    fprintf(outFile, "    char buffer[2048] = { 0 };\n");
    fprintf(outFile, "    vsnprintf(buffer, sizeof(buffer), text, args);\n");
    fprintf(outFile, "    g_mojo_trace_log_callback(logLevel, buffer);\n");
    fprintf(outFile, "}\n\n");

    fprintf(outFile, "MOJO_RAYLIB_EXPORT void mojo_raylib_SetTraceLogCallback(mojo_trace_log_callback_simple callback)\n");
    fprintf(outFile, "{\n");
    fprintf(outFile, "    g_mojo_trace_log_callback = callback;\n");
    fprintf(outFile, "    SetTraceLogCallback((callback != NULL)? mojo_trace_log_bridge : NULL);\n");
    fprintf(outFile, "}\n\n");

    fprintf(outFile, "MOJO_RAYLIB_EXPORT void mojo_raylib_TraceLogLiteral(int logLevel, const char *text)\n");
    fprintf(outFile, "{\n");
    fprintf(outFile, "    TraceLog(logLevel, \"%%s\", text);\n");
    fprintf(outFile, "}\n\n");

    fprintf(outFile, "MOJO_RAYLIB_EXPORT const char *mojo_raylib_TextFormatLiteral(const char *text)\n");
    fprintf(outFile, "{\n");
    fprintf(outFile, "    return TextFormat(\"%%s\", text);\n");
    fprintf(outFile, "}\n\n");

    for (int i = 0; i < funcCount; i++)
    {
        if (IsUnsupportedFunction(&funcs[i]))
            continue;

        fprintf(outFile, "MOJO_RAYLIB_EXPORT %s mojo_raymath_%s(", funcs[i].retType, funcs[i].name);
        for (int j = 0; j < funcs[i].paramCount; j++)
        {
            fprintf(outFile, "%s %s", funcs[i].paramType[j], funcs[i].paramName[j]);
            if (j < funcs[i].paramCount - 1)
                fprintf(outFile, ", ");
        }
        fprintf(outFile, ")\n{\n");
        if (!IsVoidType(funcs[i].retType))
            fprintf(outFile, "    return ");
        else
            fprintf(outFile, "    ");
        fprintf(outFile, "%s(", funcs[i].name);
        for (int j = 0; j < funcs[i].paramCount; j++)
        {
            fprintf(outFile, "%s", funcs[i].paramName[j]);
            if (j < funcs[i].paramCount - 1)
                fprintf(outFile, ", ");
        }
        fprintf(outFile, ");\n}\n\n");
    }

    fclose(outFile);
}

static void EnsureCodegenDirectories(const char *rootDir)
{
    char path[1024] = {0};

    EnsureDirectory(rootDir);
    JoinPath(rootDir, "mojo_raylib", path, sizeof(path));
    EnsureDirectory(path);
    JoinPath(rootDir, "mojo_raylib/raw", path, sizeof(path));
    EnsureDirectory(path);
    JoinPath(rootDir, "native", path, sizeof(path));
    EnsureDirectory(path);
}

static void EnsureDirectory(const char *dirPath)
{
    char command[1400] = {0};
    snprintf(command, sizeof(command), "mkdir -p \"%s\"", dirPath);
    system(command);
}

static void JoinPath(const char *base, const char *leaf, char *outPath, int outPathSize)
{
    snprintf(outPath, outPathSize, "%s/%s", base, leaf);
}

static void CopyText(char *dst, const char *src, int dstSize)
{
    int i = 0;
    if ((dst == NULL) || (src == NULL) || (dstSize <= 0))
        return;
    for (; (i < dstSize - 1) && (src[i] != '\0'); i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static void CopyTrimmed(char *dst, const char *src, int dstSize)
{
    int start = 0;
    int end = 0;
    int i = 0;

    if ((dst == NULL) || (src == NULL) || (dstSize <= 0))
        return;

    while ((src[start] == ' ') || (src[start] == '\t'))
        start++;
    end = TextLength(src);
    while ((end > start) && ((src[end - 1] == ' ') || (src[end - 1] == '\t')))
        end--;

    for (; (start < end) && (i < dstSize - 1); start++, i++)
        dst[i] = src[start];
    dst[i] = '\0';
}

static void ToSnakeCase(const char *source, char *outText, int outTextSize)
{
    int j = 0;

    if ((source == NULL) || (outText == NULL) || (outTextSize <= 0))
    {
        return;
    }

    for (int i = 0; source[i] != '\0' && j < outTextSize - 1; i++)
    {
        char ch = source[i];

        if ((ch == '_') || (ch == ' ') || (ch == '-'))
        {
            if ((j > 0) && (outText[j - 1] != '_'))
                outText[j++] = '_';
            continue;
        }

        if ((ch >= 'A') && (ch <= 'Z'))
        {
            char prev = (i > 0) ? source[i - 1] : '\0';
            char next = source[i + 1];
            bool prevIsLowerOrDigit = ((prev >= 'a') && (prev <= 'z')) || ((prev >= '0') && (prev <= '9'));
            bool prevIsUpper = (prev >= 'A') && (prev <= 'Z');
            bool nextIsLower = (next >= 'a') && (next <= 'z');

            if (((prevIsLowerOrDigit) || (prevIsUpper && nextIsLower)) &&
                (j > 0) && (outText[j - 1] != '_'))
                outText[j++] = '_';
            outText[j++] = (char)(ch - 'A' + 'a');
            continue;
        }

        outText[j++] = ch;
    }

    if ((j > 0) && (outText[j - 1] == '_'))
        j--;
    outText[j] = '\0';

    // Suffix Mojo reserved keywords with `_` so they don't blow up at parse time.
    static const char *kMojoReserved[] = {
        "from", "import", "def", "fn", "struct", "var", "let", "if", "else",
        "for", "while", "return", "True", "False", "None", "as", "with", "try",
        "except", "raise", "yield", "class", "lambda", "not", "and", "or", "is",
        "in", "pass", "break", "continue", "global", "nonlocal", "assert", "del",
        "out", "mut", "inout", "owned", "borrowed", "ref", "alias", "trait",
        "type", NULL
    };
    for (int k = 0; kMojoReserved[k] != NULL; k++)
    {
        unsigned int klen = TextLength(kMojoReserved[k]);
        if ((j == (int)klen) && IsTextEqual(outText, kMojoReserved[k], klen + 1))
        {
            if (j < outTextSize - 1)
            {
                outText[j++] = '_';
                outText[j] = '\0';
            }
            break;
        }
    }
}

static bool StartsWith(const char *text, const char *prefix)
{
    return IsTextEqual(text, prefix, TextLength(prefix));
}

static int CountCharOccurrences(const char *text, char ch)
{
    int count = 0;
    for (unsigned int i = 0; i < TextLength(text); i++)
        if (text[i] == ch)
            count++;
    return count;
}

static bool IsRaymathApi(void)
{
    return IsTextEqual(apiDefine, "RMAPI", 5);
}

static bool IsKnownStructOrAlias(const char *typeName)
{
    // Hard-coded opaque forward decls we synthesize stubs for at the top of the
    // generated type modules.
    static const char *kOpaque[] = { "rAudioBuffer", "rAudioProcessor", NULL };
    for (int k = 0; kOpaque[k] != NULL; k++)
    {
        unsigned int len = TextLength(kOpaque[k]);
        if (IsTextEqual(typeName, kOpaque[k], len + 1))
            return true;
    }
    for (int i = 0; i < structCount; i++)
        if (IsTextEqual(structs[i].name, typeName, TextLength(structs[i].name) + 1))
            return true;
    for (int i = 0; i < aliasCount; i++)
    {
        const char *aliasName = aliases[i].name;
        if (aliasName[0] == '*')
            aliasName++;
        if (IsTextEqual(aliasName, typeName, TextLength(aliasName) + 1))
            return true;
    }
    return false;
}

static bool IsKnownCallback(const char *typeName)
{
    for (int i = 0; i < callbackCount; i++)
        if (IsTextEqual(callbacks[i].name, typeName, TextLength(callbacks[i].name) + 1))
            return true;
    return false;
}

static bool IsVoidType(const char *typeName)
{
    char trimmed[128] = {0};
    CopyTrimmed(trimmed, typeName, sizeof(trimmed));
    return IsTextEqual(trimmed, "void", 5);
}

static bool IsUnsupportedFunction(const FunctionInfo *func)
{
    static const char *kUnsupported[] = {
        // Variadic / log helpers
        "TraceLog",
        "TextFormat",
        "SetTraceLogCallback",
        // float3/float16 returns — no Mojo equivalents are emitted.
        "Vector3ToFloatV",
        "MatrixToFloatV",
        // raymath functions taking Vector3/Quaternion out-pointers — the safe
        // wrappers can't bridge UnsafePointer[Vector3, ...] cleanly today.
        "Vector3OrthoNormalize",
        "QuaternionToAxisAngle",
        "MatrixDecompose",
        // Callback setters whose callback type aliases aren't generated yet.
        "SetLoadFileDataCallback",
        "SetSaveFileDataCallback",
        "SetLoadFileTextCallback",
        "SetSaveFileTextCallback",
        // Misc origin-mismatch sites until codegen learns to thread origins.
        "SetWindowIcons",
        "LoadFileData",
        "ExportDataAsCode",
        // VrStereoConfig contains InlineArray[Matrix, 2] which can't be
        // TrivialRegisterPassable, so external_call refuses it as a return value.
        "LoadVrStereoConfig",
        "BeginVrStereoMode",
        "UnloadVrStereoConfig",
        NULL
    };
    for (int i = 0; kUnsupported[i] != NULL; i++)
    {
        unsigned int len = TextLength(kUnsupported[i]);
        if (IsTextEqual(func->name, kUnsupported[i], len + 1))
            return true;
    }

    for (int i = 0; i < func->paramCount; i++)
    {
        if (IsTextEqual(func->paramType[i], "...", 4))
            return true;
        if (TextFindIndex(func->paramType[i], "va_list") > -1)
            return true;
    }
    return false;
}

static bool IsMutablePointerType(const char *typeName)
{
    char trimmed[128] = {0};
    CopyTrimmed(trimmed, typeName, sizeof(trimmed));
    if (TextFindIndex(trimmed, "*") < 0)
        return false;
    return !StartsWith(trimmed, "const ");
}

static bool IsIntegralCountType(const char *typeName)
{
    char trimmed[128] = {0};
    CopyTrimmed(trimmed, typeName, sizeof(trimmed));
    return IsTextEqual(trimmed, "int", 4) || IsTextEqual(trimmed, "unsigned int", 13);
}

static bool IsCountLikeName(const char *name)
{
    char camel[64] = {0};
    ToSnakeCase(name, camel, sizeof(camel));
    return (TextFindIndex(camel, "count") > -1) ||
           (TextFindIndex(camel, "size") > -1) ||
           (TextFindIndex(camel, "length") > -1);
}

static bool FunctionHasMemFreeOwnership(const FunctionInfo *func)
{
    return (TextFindIndex(func->desc, "MemFree()") > -1);
}

static int FindFunctionIndexByName(const char *name)
{
    for (int i = 0; i < funcCount; i++)
    {
        if (IsTextEqual(funcs[i].name, name, TextLength(funcs[i].name) + 1))
            return i;
    }
    return -1;
}

static int FindMatchingReleaseFunction(const FunctionInfo *func, char *releaseName, int releaseNameSize)
{
    char unloadName[128] = {0};
    if (TextFindIndex(func->retType, "*") < 0)
        return 0;

    if (StartsWith(func->name, "Load"))
    {
        snprintf(unloadName, sizeof(unloadName), "Unload%s", func->name + 4);
        if (FindFunctionIndexByName(unloadName) > -1)
        {
            CopyText(releaseName, unloadName, releaseNameSize);
            return 1;
        }
    }

    if (FunctionHasMemFreeOwnership(func))
    {
        CopyText(releaseName, "MemFree", releaseNameSize);
        return 2;
    }

    return 0;
}

static void GetOwnedStructName(const FunctionInfo *func, char *structName, int structNameSize)
{
    if (StartsWith(func->name, "Load"))
        snprintf(structName, structNameSize, "Owned%s", func->name + 4);
    else
        snprintf(structName, structNameSize, "Owned%s", func->name);
}

static void GetMojoType(const char *cType, bool forReturnType, bool forCallbackParam, char *outType, int outTypeSize)
{
    char trimmed[128] = {0};
    char baseType[128] = {0};
    char scalarType[128] = {0};
    int pointerCount = 0;
    bool isConst = false;
    int arrayStart = TextFindIndex(cType, "[");

    CopyTrimmed(trimmed, cType, sizeof(trimmed));

    if (arrayStart > -1)
    {
        char arraySizeText[32] = {0};
        char trimmedBase[128] = {0};
        int arrayEnd = TextFindIndex(trimmed, "]");
        int arraySize = 0;
        for (int i = 0; i < arrayStart; i++)
            baseType[i] = trimmed[i];
        baseType[arrayStart] = '\0';
        if (arrayEnd > arrayStart)
        {
            for (int i = arrayStart + 1, j = 0; i < arrayEnd; i++, j++)
                arraySizeText[j] = trimmed[i];
            arraySize = atoi(arraySizeText);
        }
        // Use SIMD for numeric arrays so the enclosing struct can be RegisterPassable
        // (InlineArray itself isn't conditionally RP in Mojo 0.26).
        CopyTrimmed(trimmedBase, baseType, sizeof(trimmedBase));
        const char *dtype = NULL;
        if (IsTextEqual(trimmedBase, "char", 5))                dtype = "DType.int8";
        else if (IsTextEqual(trimmedBase, "unsigned char", 14)) dtype = "DType.uint8";
        else if (IsTextEqual(trimmedBase, "short", 6))          dtype = "DType.int16";
        else if (IsTextEqual(trimmedBase, "unsigned short", 15))dtype = "DType.uint16";
        else if (IsTextEqual(trimmedBase, "int", 4))            dtype = "DType.int32";
        else if (IsTextEqual(trimmedBase, "unsigned int", 13))  dtype = "DType.uint32";
        else if (IsTextEqual(trimmedBase, "long", 5))           dtype = "DType.int64";
        else if (IsTextEqual(trimmedBase, "unsigned long", 14)) dtype = "DType.uint64";
        else if (IsTextEqual(trimmedBase, "float", 6))          dtype = "DType.float32";
        else if (IsTextEqual(trimmedBase, "double", 7))         dtype = "DType.float64";

        if (dtype != NULL)
        {
            snprintf(outType, outTypeSize, "SIMD[%s, %i]", dtype, arraySize);
            return;
        }
        GetMojoType(baseType, false, false, scalarType, sizeof(scalarType));
        snprintf(outType, outTypeSize, "InlineArray[%s, %i]", scalarType, arraySize);
        return;
    }

    if (StartsWith(trimmed, "const "))
    {
        isConst = true;
        CopyTrimmed(baseType, trimmed + 6, sizeof(baseType));
    }
    else
        CopyTrimmed(baseType, trimmed, sizeof(baseType));

    pointerCount = CountCharOccurrences(baseType, '*');
    if (pointerCount > 0)
    {
        char noStars[128] = {0};
        int j = 0;
        for (unsigned int i = 0; i < TextLength(baseType); i++)
        {
            if (baseType[i] != '*')
                noStars[j++] = baseType[i];
        }
        noStars[j] = '\0';
        CopyTrimmed(baseType, noStars, sizeof(baseType));
    }

    if (IsTextEqual(baseType, "void", 5))
    {
        if (pointerCount == 0)
        {
            CopyText(outType, "NoneType", outTypeSize);
            return;
        }

        snprintf(outType, outTypeSize, "UnsafePointer[NoneType, MutAnyOrigin]");
        return;
    }
    else if (IsTextEqual(baseType, "char", 5))
        CopyText(scalarType, "c_char", sizeof(scalarType));
    else if (IsTextEqual(baseType, "unsigned char", 14))
        CopyText(scalarType, "c_uchar", sizeof(scalarType));
    else if (IsTextEqual(baseType, "short", 6))
        CopyText(scalarType, "c_short", sizeof(scalarType));
    else if (IsTextEqual(baseType, "unsigned short", 15))
        CopyText(scalarType, "c_ushort", sizeof(scalarType));
    else if (IsTextEqual(baseType, "int", 4))
        CopyText(scalarType, "c_int", sizeof(scalarType));
    else if (IsTextEqual(baseType, "unsigned int", 13))
        CopyText(scalarType, "c_uint", sizeof(scalarType));
    else if (IsTextEqual(baseType, "long", 5))
        CopyText(scalarType, "c_long", sizeof(scalarType));
    else if (IsTextEqual(baseType, "unsigned long", 14))
        CopyText(scalarType, "c_ulong", sizeof(scalarType));
    else if (IsTextEqual(baseType, "float", 6))
        CopyText(scalarType, "c_float", sizeof(scalarType));
    else if (IsTextEqual(baseType, "double", 7))
        CopyText(scalarType, "c_double", sizeof(scalarType));
    else if (IsTextEqual(baseType, "bool", 5))
        CopyText(scalarType, "Bool", sizeof(scalarType));
    else if (IsTextEqual(baseType, "va_list", 8))
        CopyText(scalarType, "UnsafePointer[NoneType, MutAnyOrigin]", sizeof(scalarType));
    else if (IsKnownStructOrAlias(baseType))
        CopyText(scalarType, baseType, sizeof(scalarType));
    else
        CopyText(scalarType, baseType, sizeof(scalarType));

    if (pointerCount == 0)
    {
        CopyText(outType, scalarType, outTypeSize);
        return;
    }

    if (forCallbackParam && IsTextEqual(scalarType, "c_char", 7))
    {
        snprintf(outType, outTypeSize, "UnsafePointer[c_char, MutAnyOrigin]");
        return;
    }

    if (!forReturnType && !forCallbackParam && isConst && IsTextEqual(scalarType, "c_char", 7))
    {
        CopyText(outType, "CStringSlice", outTypeSize);
        return;
    }

    if (!forReturnType && IsTextEqual(scalarType, "c_char", 7))
    {
        snprintf(outType, outTypeSize, "UnsafePointer[c_char, MutAnyOrigin]");
        return;
    }

    snprintf(outType, outTypeSize, "UnsafePointer[%s, MutAnyOrigin]", scalarType);
}

static void GetMojoPublicType(const char *cType, bool forReturnType, bool forCallbackParam, char *outType, int outTypeSize)
{
    char trimmed[128] = {0};
    char baseType[128] = {0};
    char scalarType[128] = {0};
    int pointerCount = 0;
    bool isConst = false;
    int arrayStart = TextFindIndex(cType, "[");

    CopyTrimmed(trimmed, cType, sizeof(trimmed));

    if (arrayStart > -1)
    {
        GetMojoType(cType, forReturnType, forCallbackParam, outType, outTypeSize);
        return;
    }

    if (StartsWith(trimmed, "const "))
    {
        isConst = true;
        CopyTrimmed(baseType, trimmed + 6, sizeof(baseType));
    }
    else
        CopyTrimmed(baseType, trimmed, sizeof(baseType));

    pointerCount = CountCharOccurrences(baseType, '*');
    if (pointerCount > 0)
    {
        if (isConst && IsTextEqual(baseType, "char *", 7))
        {
            if (forReturnType)
                CopyText(outType, "String", outTypeSize);
            else
                CopyText(outType, "String", outTypeSize);
            return;
        }

        GetMojoType(cType, forReturnType, forCallbackParam, outType, outTypeSize);
        return;
    }

    if (IsTextEqual(baseType, "char", 5))
        CopyText(scalarType, "Int8", sizeof(scalarType));
    else if (IsTextEqual(baseType, "unsigned char", 14))
        CopyText(scalarType, "UInt8", sizeof(scalarType));
    else if (IsTextEqual(baseType, "short", 6))
        CopyText(scalarType, "Int16", sizeof(scalarType));
    else if (IsTextEqual(baseType, "unsigned short", 15))
        CopyText(scalarType, "UInt16", sizeof(scalarType));
    else if (IsTextEqual(baseType, "int", 4))
        CopyText(scalarType, "Int", sizeof(scalarType));
    else if (IsTextEqual(baseType, "unsigned int", 13))
        CopyText(scalarType, "UInt", sizeof(scalarType));
    else if (IsTextEqual(baseType, "long", 5))
        CopyText(scalarType, "Int", sizeof(scalarType));
    else if (IsTextEqual(baseType, "unsigned long", 14))
        CopyText(scalarType, "UInt", sizeof(scalarType));
    else if (IsTextEqual(baseType, "float", 6))
        CopyText(scalarType, "Float32", sizeof(scalarType));
    else if (IsTextEqual(baseType, "double", 7))
        CopyText(scalarType, "Float64", sizeof(scalarType));
    else if (IsTextEqual(baseType, "bool", 5))
        CopyText(scalarType, "Bool", sizeof(scalarType));
    else if (IsKnownCallback(baseType))
        snprintf(scalarType, sizeof(scalarType), "raw_types.%s", baseType);
    else
        CopyText(scalarType, baseType, sizeof(scalarType));

    CopyText(outType, scalarType, outTypeSize);
}

static void GetMojoLayoutType(const char *cType, char *outType, int outTypeSize)
{
    char trimmed[128] = {0};
    char baseType[128] = {0};
    bool isConst = false;
    int pointerCount = 0;
    int arrayStart = TextFindIndex(cType, "[");

    CopyTrimmed(trimmed, cType, sizeof(trimmed));

    if (arrayStart > -1)
    {
        char arrayBaseType[128] = {0};
        char arrayElementType[128] = {0};
        char arraySizeText[32] = {0};
        int arrayEnd = TextFindIndex(trimmed, "]");
        int arraySize = 0;
        for (int i = 0; i < arrayStart; i++)
            arrayBaseType[i] = trimmed[i];
        arrayBaseType[arrayStart] = '\0';
        if (arrayEnd > arrayStart)
        {
            for (int i = arrayStart + 1, j = 0; i < arrayEnd; i++, j++)
                arraySizeText[j] = trimmed[i];
            arraySize = atoi(arraySizeText);
        }
        // Mirror GetMojoType: SIMD for numeric so the public struct can also be RegisterPassable.
        char trimmedAB[128] = {0};
        CopyTrimmed(trimmedAB, arrayBaseType, sizeof(trimmedAB));
        const char *dtype = NULL;
        if (IsTextEqual(trimmedAB, "char", 5))                dtype = "DType.int8";
        else if (IsTextEqual(trimmedAB, "unsigned char", 14)) dtype = "DType.uint8";
        else if (IsTextEqual(trimmedAB, "short", 6))          dtype = "DType.int16";
        else if (IsTextEqual(trimmedAB, "unsigned short", 15))dtype = "DType.uint16";
        else if (IsTextEqual(trimmedAB, "int", 4))            dtype = "DType.int32";
        else if (IsTextEqual(trimmedAB, "unsigned int", 13))  dtype = "DType.uint32";
        else if (IsTextEqual(trimmedAB, "long", 5))           dtype = "DType.int64";
        else if (IsTextEqual(trimmedAB, "unsigned long", 14)) dtype = "DType.uint64";
        else if (IsTextEqual(trimmedAB, "float", 6))          dtype = "DType.float32";
        else if (IsTextEqual(trimmedAB, "double", 7))         dtype = "DType.float64";
        if (dtype != NULL)
        {
            snprintf(outType, outTypeSize, "SIMD[%s, %i]", dtype, arraySize);
            return;
        }
        GetMojoLayoutType(arrayBaseType, arrayElementType, sizeof(arrayElementType));
        snprintf(outType, outTypeSize, "InlineArray[%s, %i]", arrayElementType, arraySize);
        return;
    }

    if (!GetPointerBaseType(trimmed, baseType, sizeof(baseType), &isConst, &pointerCount))
    {
        CopyTrimmed(baseType, trimmed, sizeof(baseType));
    }

    if (pointerCount > 0)
    {
        GetMojoType(cType, false, false, outType, outTypeSize);
        return;
    }

    if (IsTextEqual(baseType, "char", 5))
        CopyText(outType, "Int8", outTypeSize);
    else if (IsTextEqual(baseType, "unsigned char", 14))
        CopyText(outType, "UInt8", outTypeSize);
    else if (IsTextEqual(baseType, "short", 6))
        CopyText(outType, "Int16", outTypeSize);
    else if (IsTextEqual(baseType, "unsigned short", 15))
        CopyText(outType, "UInt16", outTypeSize);
    else if (IsTextEqual(baseType, "int", 4))
        CopyText(outType, "Int", outTypeSize);
    else if (IsTextEqual(baseType, "unsigned int", 13))
        CopyText(outType, "UInt", outTypeSize);
    else if (IsTextEqual(baseType, "long", 5))
        CopyText(outType, "Int", outTypeSize);
    else if (IsTextEqual(baseType, "unsigned long", 14))
        CopyText(outType, "UInt", outTypeSize);
    else if (IsTextEqual(baseType, "float", 6))
        CopyText(outType, "Float32", outTypeSize);
    else if (IsTextEqual(baseType, "double", 7))
        CopyText(outType, "Float64", outTypeSize);
    else if (IsTextEqual(baseType, "bool", 5))
        CopyText(outType, "Bool", outTypeSize);
    else
        CopyText(outType, baseType, outTypeSize);
}

static bool IsPointerAlias(const char *typeName)
{
    for (int i = 0; i < aliasCount; i++)
    {
        if (aliases[i].name[0] == '*')
        {
            const char *alias = aliases[i].name + 1;
            if (IsTextEqual(alias, typeName, TextLength(alias) + 1))
                return true;
        }
    }
    return false;
}

static void ResolveStructOrAliasName(const char *typeName, char *outTypeName, int outTypeNameSize)
{
    char current[128] = {0};
    CopyTrimmed(current, typeName, sizeof(current));

    while (true)
    {
        bool resolved = false;
        for (int i = 0; i < aliasCount; i++)
        {
            char aliasName[128] = {0};
            int start = (aliases[i].name[0] == '*') ? 1 : 0;
            CopyText(aliasName, aliases[i].name + start, sizeof(aliasName));
            if (IsTextEqual(aliasName, current, TextLength(aliasName) + 1))
            {
                char next[128] = {0};
                CopyTrimmed(next, aliases[i].type, sizeof(next));
                if (IsTextEqual(next, current, TextLength(next) + 1))
                    break;
                CopyText(current, next, sizeof(current));
                resolved = true;
                break;
            }
        }
        if (!resolved)
            break;
    }

    CopyText(outTypeName, current, outTypeNameSize);
}

static bool GetPointerBaseType(const char *typeName, char *baseType, int baseTypeSize, bool *isConst, int *pointerCount)
{
    char trimmed[128] = {0};
    char localBase[128] = {0};
    int localPointerCount = 0;

    CopyTrimmed(trimmed, typeName, sizeof(trimmed));
    *isConst = false;
    if (StartsWith(trimmed, "const "))
    {
        *isConst = true;
        CopyTrimmed(localBase, trimmed + 6, sizeof(localBase));
    }
    else
        CopyTrimmed(localBase, trimmed, sizeof(localBase));

    localPointerCount = CountCharOccurrences(localBase, '*');
    if (localPointerCount == 0)
    {
        if (pointerCount != NULL)
            *pointerCount = 0;
        CopyText(baseType, localBase, baseTypeSize);
        return false;
    }

    char noStars[128] = {0};
    int j = 0;
    for (unsigned int i = 0; i < TextLength(localBase); i++)
    {
        if (localBase[i] != '*')
            noStars[j++] = localBase[i];
    }
    noStars[j] = '\0';
    CopyTrimmed(baseType, noStars, baseTypeSize);
    if (pointerCount != NULL)
        *pointerCount = localPointerCount;
    return true;
}

static bool GetSpanCompanionIndex(const FunctionInfo *func, int paramIndex, int *countIndex)
{
    bool isConst = false;
    int pointerCount = 0;
    char baseType[128] = {0};

    if (!GetPointerBaseType(func->paramType[paramIndex], baseType, sizeof(baseType), &isConst, &pointerCount))
        return false;
    if (pointerCount != 1)
        return false;
    if (IsTextEqual(baseType, "void", 5))
        return false;
    // char pointers are strings, not arrays — handled via CStringSlice/String,
    // never paired with a count parameter.
    if (IsTextEqual(baseType, "char", 5))
        return false;

    if ((paramIndex + 1 < func->paramCount) &&
        IsIntegralCountType(func->paramType[paramIndex + 1]) &&
        IsCountLikeName(func->paramName[paramIndex + 1]))
    {
        *countIndex = paramIndex + 1;
        return true;
    }

    return false;
}

static bool ShouldUseMutValueParam(const FunctionInfo *func, int paramIndex, char *pointeeType, int pointeeTypeSize)
{
    bool isConst = false;
    int pointerCount = 0;
    int countIndex = -1;

    if (!GetPointerBaseType(func->paramType[paramIndex], pointeeType, pointeeTypeSize, &isConst, &pointerCount))
        return false;
    if (isConst || (pointerCount != 1))
        return false;
    if (GetSpanCompanionIndex(func, paramIndex, &countIndex))
        return false;
    if (!IsKnownStructOrAlias(pointeeType))
        return false;
    return true;
}

static bool ShouldUseRefValueParam(const FunctionInfo *func, int paramIndex, char *pointeeType, int pointeeTypeSize)
{
    bool isConst = false;
    int pointerCount = 0;
    int countIndex = -1;

    if (!GetPointerBaseType(func->paramType[paramIndex], pointeeType, pointeeTypeSize, &isConst, &pointerCount))
        return false;
    if (!isConst || (pointerCount != 1))
        return false;
    if (GetSpanCompanionIndex(func, paramIndex, &countIndex))
        return false;
    if (!IsKnownStructOrAlias(pointeeType))
        return false;
    return true;
}

static void GetMojoPublicSpanType(const char *pointerType, char *outType, int outTypeSize)
{
    char baseType[128] = {0};
    char elementType[128] = {0};
    bool isConst = false;
    int pointerCount = 0;

    if (!GetPointerBaseType(pointerType, baseType, sizeof(baseType), &isConst, &pointerCount))
    {
        GetMojoPublicType(pointerType, false, false, outType, outTypeSize);
        return;
    }

    GetMojoLayoutType(baseType, elementType, sizeof(elementType));
    snprintf(outType, outTypeSize, "Span[%s, _]", elementType);
}

static void GetMojoAliasTarget(const AliasInfo *alias, char *outType, int outTypeSize)
{
    char targetType[256] = {0};
    char pointedName[128] = {0};
    if (alias->name[0] == '*')
    {
        CopyTrimmed(pointedName, alias->type, sizeof(pointedName));
        snprintf(outType, outTypeSize, "UnsafePointer[%s, MutAnyOrigin]", pointedName);
        return;
    }

    GetMojoType(alias->type, false, false, targetType, sizeof(targetType));
    CopyText(outType, targetType, outTypeSize);
}

static void SanitizeComment(const char *source, char *outComment, int outCommentSize)
{
    int j = 0;
    if ((source == NULL) || (outComment == NULL) || (outCommentSize <= 0))
        return;

    for (int i = 0; (source[i] != '\0') && (j < outCommentSize - 1); i++)
    {
        if ((source[i] == '\n') || (source[i] == '\r') || (source[i] == '\t'))
            outComment[j++] = ' ';
        else
            outComment[j++] = source[i];
    }
    outComment[j] = '\0';
}

static void GetRelativeSourceName(const char *sourceName, char *outSourceName, int outSourceNameSize)
{
    int relativeIndex = TextFindIndex(sourceName, "vendor/raylib/");
    if (relativeIndex > -1)
    {
        CopyText(outSourceName, sourceName + relativeIndex + 14, outSourceNameSize);
        return;
    }

    relativeIndex = TextFindIndex(sourceName, "src/");
    if (relativeIndex > -1)
    {
        CopyText(outSourceName, sourceName + relativeIndex, outSourceNameSize);
        return;
    }

    CopyText(outSourceName, sourceName, outSourceNameSize);
}

static void WriteGeneratedHeader(FILE *outFile, const char *title, const char *sourceName)
{
    char relativeSource[512] = {0};
    GetRelativeSourceName(sourceName, relativeSource, sizeof(relativeSource));
    fprintf(outFile, "# Auto-generated by rlparser CODE mode.\n");
    fprintf(outFile, "# %s\n", title);
    fprintf(outFile, "# Source: %s\n\n", relativeSource);
}

static void WriteMojoCommonImports(FILE *outFile)
{
    fprintf(outFile, "from std.ffi import CStringSlice, c_char, c_uchar, c_short, c_ushort, c_int, c_uint, c_long, c_ulong, c_float, c_double, external_call\n");
    fprintf(outFile, "from std.memory.unsafe_pointer import UnsafePointer\n");
    fprintf(outFile, "from std.collections import InlineArray\n\n");
}

static void WriteMojoDocstring(FILE *outFile, const char *comment, int indentLevel)
{
    char sanitized[640] = {0};
    SanitizeComment(comment, sanitized, sizeof(sanitized));
    if (sanitized[0] == '\0')
        return;

    for (int i = 0; sanitized[i] != '\0'; i++)
    {
        if (sanitized[i] == '"')
            sanitized[i] = '\'';
    }

    for (int i = 0; i < indentLevel; i++)
        fputc(' ', outFile);
    fprintf(outFile, "\"\"\"%s\"\"\"\n", sanitized);
}

static const char *EscapeMojoKeyword(const char *name, char *buffer, int bufferSize)
{
    static const char *kMojoReserved[] = {
        "from", "import", "def", "fn", "struct", "var", "let", "if", "else",
        "for", "while", "return", "True", "False", "None", "as", "with", "try",
        "except", "raise", "yield", "class", "lambda", "not", "and", "or", "is",
        "in", "pass", "break", "continue", "global", "nonlocal", "assert", "del",
        "out", "mut", "inout", "owned", "borrowed", "ref", "alias", "trait",
        "type", NULL
    };
    if ((name == NULL) || (buffer == NULL) || (bufferSize <= 0))
        return name;
    unsigned int len = TextLength(name);
    for (int k = 0; kMojoReserved[k] != NULL; k++)
    {
        unsigned int klen = TextLength(kMojoReserved[k]);
        if ((len == klen) && IsTextEqual(name, kMojoReserved[k], klen + 1))
        {
            snprintf(buffer, bufferSize, "%s_", name);
            return buffer;
        }
    }
    return name;
}

static void WriteMojoSignatureParams(FILE *outFile, const FunctionInfo *func, bool forCallbackParam)
{
    for (int i = 0; i < func->paramCount; i++)
    {
        char mojoType[256] = {0};
        char escaped[64] = {0};
        if (IsTextEqual(func->paramType[i], "...", 4))
            continue;
        GetMojoType(func->paramType[i], false, forCallbackParam, mojoType, sizeof(mojoType));
        fprintf(outFile, "%s: %s", EscapeMojoKeyword(func->paramName[i], escaped, sizeof(escaped)), mojoType);
        if (i < func->paramCount - 1)
            fprintf(outFile, ", ");
    }
}

static void WriteMojoCallArgs(FILE *outFile, const FunctionInfo *func)
{
    for (int i = 0; i < func->paramCount; i++)
    {
        char escaped[64] = {0};
        fprintf(outFile, "%s", EscapeMojoKeyword(func->paramName[i], escaped, sizeof(escaped)));
        if (i < func->paramCount - 1)
            fprintf(outFile, ", ");
    }
}

static void WritePublicToRawArg(FILE *outFile, const char *cType, const char *expr)
{
    char trimmed[128] = {0};
    char baseType[128] = {0};
    int pointerCount = 0;
    bool isConst = false;

    CopyTrimmed(trimmed, cType, sizeof(trimmed));
    if (StartsWith(trimmed, "const "))
    {
        isConst = true;
        CopyTrimmed(baseType, trimmed + 6, sizeof(baseType));
    }
    else
        CopyTrimmed(baseType, trimmed, sizeof(baseType));

    pointerCount = CountCharOccurrences(baseType, '*');
    if (pointerCount > 0)
    {
        char strippedBase[128] = {0};
        int j = 0;
        for (unsigned int i = 0; i < TextLength(baseType); i++)
        {
            if (baseType[i] != '*')
                strippedBase[j++] = baseType[i];
        }
        strippedBase[j] = '\0';
        char trimmedBase[128] = {0};
        CopyTrimmed(trimmedBase, strippedBase, sizeof(trimmedBase));

        if (isConst && IsTextEqual(baseType, "char *", 7))
            fprintf(outFile, "CStringSlice(unsafe_from_ptr=%s.unsafe_ptr().bitcast[c_char]())", expr);
        else if (IsKnownStructOrAlias(trimmedBase))
        {
            // For pointer aliases (e.g. ModelAnimPose = Transform*), keep the alias name
            // so we cast to the same logical pointee. Resolving would lose a pointer level.
            char target[128] = {0};
            if (IsPointerAlias(trimmedBase))
                CopyText(target, trimmedBase, sizeof(target));
            else
                ResolveStructOrAliasName(trimmedBase, target, sizeof(target));
            fprintf(outFile,
                "UnsafePointer(to=%s).bitcast[raw_types.%s]().mut_cast[True]().as_any_origin()",
                expr, target);
        }
        else
            fprintf(outFile, "%s", expr);
        return;
    }

    if (IsKnownStructOrAlias(baseType))
    {
        char snakeName[128] = {0};
        ToSnakeCase(baseType, snakeName, sizeof(snakeName));
        fprintf(outFile, "public_types._to_raw_%s(%s)", snakeName, expr);
        return;
    }

    if (IsTextEqual(baseType, "char", 5))
        fprintf(outFile, "c_char(%s)", expr);
    else if (IsTextEqual(baseType, "unsigned char", 14))
        fprintf(outFile, "c_uchar(%s)", expr);
    else if (IsTextEqual(baseType, "short", 6))
        fprintf(outFile, "c_short(%s)", expr);
    else if (IsTextEqual(baseType, "unsigned short", 15))
        fprintf(outFile, "c_ushort(%s)", expr);
    else if (IsTextEqual(baseType, "int", 4))
        fprintf(outFile, "c_int(%s)", expr);
    else if (IsTextEqual(baseType, "unsigned int", 13))
        fprintf(outFile, "c_uint(%s)", expr);
    else if (IsTextEqual(baseType, "long", 5))
        fprintf(outFile, "c_long(%s)", expr);
    else if (IsTextEqual(baseType, "unsigned long", 14))
        fprintf(outFile, "c_ulong(%s)", expr);
    else if (IsTextEqual(baseType, "float", 6))
        fprintf(outFile, "c_float(%s)", expr);
    else if (IsTextEqual(baseType, "double", 7))
        fprintf(outFile, "c_double(%s)", expr);
    else
        fprintf(outFile, "%s", expr);
}

static void WriteRawToPublicExpr(FILE *outFile, const char *cType, const char *expr)
{
    char trimmed[128] = {0};
    char baseType[128] = {0};
    int pointerCount = 0;
    bool isConst = false;

    CopyTrimmed(trimmed, cType, sizeof(trimmed));
    if (StartsWith(trimmed, "const "))
    {
        isConst = true;
        CopyTrimmed(baseType, trimmed + 6, sizeof(baseType));
    }
    else
        CopyTrimmed(baseType, trimmed, sizeof(baseType));

    pointerCount = CountCharOccurrences(baseType, '*');
    if (pointerCount > 0)
    {
        char strippedBase[128] = {0};
        int j = 0;
        for (unsigned int i = 0; i < TextLength(baseType); i++)
        {
            if (baseType[i] != '*')
                strippedBase[j++] = baseType[i];
        }
        strippedBase[j] = '\0';
        char trimmedBase[128] = {0};
        CopyTrimmed(trimmedBase, strippedBase, sizeof(trimmedBase));

        if (isConst && IsTextEqual(baseType, "char *", 7))
            fprintf(outFile, "String(CStringSlice(unsafe_from_ptr=%s))", expr);
        else if (IsKnownStructOrAlias(trimmedBase))
        {
            // Same pointer-alias caveat as WritePublicToRawArg.
            char target[128] = {0};
            if (IsPointerAlias(trimmedBase))
                CopyText(target, trimmedBase, sizeof(target));
            else
                CopyText(target, trimmedBase, sizeof(target));
            fprintf(outFile, "%s.bitcast[%s]()", expr, target);
        }
        else
            fprintf(outFile, "%s", expr);
        return;
    }

    if (IsKnownStructOrAlias(baseType))
    {
        char snakeName[128] = {0};
        ToSnakeCase(baseType, snakeName, sizeof(snakeName));
        fprintf(outFile, "public_types._from_raw_%s(%s)", snakeName, expr);
        return;
    }

    if (IsTextEqual(baseType, "char", 5))
        fprintf(outFile, "Int8(%s)", expr);
    else if (IsTextEqual(baseType, "unsigned char", 14))
        fprintf(outFile, "UInt8(%s)", expr);
    else if (IsTextEqual(baseType, "short", 6))
        fprintf(outFile, "Int16(%s)", expr);
    else if (IsTextEqual(baseType, "unsigned short", 15))
        fprintf(outFile, "UInt16(%s)", expr);
    else if (IsTextEqual(baseType, "int", 4))
        fprintf(outFile, "Int(%s)", expr);
    else if (IsTextEqual(baseType, "unsigned int", 13))
        fprintf(outFile, "UInt(%s)", expr);
    else if (IsTextEqual(baseType, "long", 5))
        fprintf(outFile, "Int(%s)", expr);
    else if (IsTextEqual(baseType, "unsigned long", 14))
        fprintf(outFile, "UInt(%s)", expr);
    else if (IsTextEqual(baseType, "float", 6))
        fprintf(outFile, "Float32(%s)", expr);
    else if (IsTextEqual(baseType, "double", 7))
        fprintf(outFile, "Float64(%s)", expr);
    else
        fprintf(outFile, "%s", expr);
}

static void WritePublicTypeToRawExpr(FILE *outFile, const char *typeName, const char *expr)
{
    char trimmed[128] = {0};
    CopyTrimmed(trimmed, typeName, sizeof(trimmed));
    if (IsPointerAlias(trimmed))
    {
        // Pointer aliases (e.g. ModelAnimPose = Transform*) — bitcast the pointer's
        // pointee from the public struct to the raw_types-qualified one.
        char target[128] = {0};
        ResolveStructOrAliasName(trimmed, target, sizeof(target));
        fprintf(outFile, "%s.bitcast[raw_types.%s]()", expr, target);
        return;
    }
    WritePublicToRawArg(outFile, typeName, expr);
}

static void WriteRawTypeToPublicExpr(FILE *outFile, const char *typeName, const char *expr)
{
    char trimmed[128] = {0};
    CopyTrimmed(trimmed, typeName, sizeof(trimmed));
    if (IsPointerAlias(trimmed))
    {
        char target[128] = {0};
        ResolveStructOrAliasName(trimmed, target, sizeof(target));
        fprintf(outFile, "%s.bitcast[%s]()", expr, target);
        return;
    }
    char resolvedName[128] = {0};
    ResolveStructOrAliasName(typeName, resolvedName, sizeof(resolvedName));
    WriteRawToPublicExpr(outFile, resolvedName, expr);
}

static void WriteSpanPointerToRawExpr(FILE *outFile, const char *pointerType, const char *expr)
{
    char baseType[128] = {0};
    bool isConst = false;
    int pointerCount = 0;
    const char *originSuffix = ".mut_cast[True]().as_any_origin()";
    (void)isConst;

    if (!GetPointerBaseType(pointerType, baseType, sizeof(baseType), &isConst, &pointerCount))
    {
        fprintf(outFile, "%s.unsafe_ptr()%s", expr, originSuffix);
        return;
    }

    // Recompute origin suffix now that isConst is known.
    originSuffix = ".mut_cast[True]().as_any_origin()";

    if (IsKnownStructOrAlias(baseType))
    {
        char resolvedName[128] = {0};
        ResolveStructOrAliasName(baseType, resolvedName, sizeof(resolvedName));
        fprintf(outFile, "%s.unsafe_ptr().bitcast[raw_types.%s]()%s", expr, resolvedName, originSuffix);
        return;
    }

    if (IsTextEqual(baseType, "int", 4))
        fprintf(outFile, "%s.unsafe_ptr().bitcast[c_int]()%s", expr, originSuffix);
    else if (IsTextEqual(baseType, "unsigned int", 13))
        fprintf(outFile, "%s.unsafe_ptr().bitcast[c_uint]()%s", expr, originSuffix);
    else if (IsTextEqual(baseType, "short", 6))
        fprintf(outFile, "%s.unsafe_ptr().bitcast[c_short]()%s", expr, originSuffix);
    else if (IsTextEqual(baseType, "unsigned short", 15))
        fprintf(outFile, "%s.unsafe_ptr().bitcast[c_ushort]()%s", expr, originSuffix);
    else if (IsTextEqual(baseType, "char", 5))
        fprintf(outFile, "%s.unsafe_ptr().bitcast[c_char]()%s", expr, originSuffix);
    else if (IsTextEqual(baseType, "unsigned char", 14))
        fprintf(outFile, "%s.unsafe_ptr().bitcast[c_uchar]()%s", expr, originSuffix);
    else if (IsTextEqual(baseType, "float", 6))
        fprintf(outFile, "%s.unsafe_ptr().bitcast[c_float]()%s", expr, originSuffix);
    else if (IsTextEqual(baseType, "double", 7))
        fprintf(outFile, "%s.unsafe_ptr().bitcast[c_double]()%s", expr, originSuffix);
    else
        fprintf(outFile, "%s.unsafe_ptr()%s", expr, originSuffix);
}

// Export parsed data in desired format
static void ExportParsedData(const char *fileName, int format)
{
    FILE *outFile = NULL;

    if (format == CODE)
    {
        ExportCodeBundle(fileName);
        return;
    }

    outFile = fopen(fileName, "wt");

    switch (format)
    {
    case DEFAULT:
    {
        // Print defines info
        fprintf(outFile, "\nDefines found: %i\n\n", defineCount);
        for (int i = 0; i < defineCount; i++)
        {
            fprintf(outFile, "Define %03i: %s\n", i + 1, defines[i].name);
            fprintf(outFile, "  Name: %s\n", defines[i].name);
            fprintf(outFile, "  Type: %s\n", StrDefineType(defines[i].type));
            fprintf(outFile, "  Value: %s\n", defines[i].value);
            fprintf(outFile, "  Description: %s\n", defines[i].desc);
        }

        // Print structs info
        fprintf(outFile, "\nStructures found: %i\n\n", structCount);
        for (int i = 0; i < structCount; i++)
        {
            fprintf(outFile, "Struct %02i: %s (%i fields)\n", i + 1, structs[i].name, structs[i].fieldCount);
            fprintf(outFile, "  Name: %s\n", structs[i].name);
            fprintf(outFile, "  Description: %s\n", structs[i].desc);
            for (int f = 0; f < structs[i].fieldCount; f++)
            {
                fprintf(outFile, "  Field[%i]: %s %s ", f + 1, structs[i].fieldType[f], structs[i].fieldName[f]);
                if (structs[i].fieldDesc[f][0])
                    fprintf(outFile, "// %s\n", structs[i].fieldDesc[f]);
                else
                    fprintf(outFile, "\n");
            }
        }

        // Print aliases info
        fprintf(outFile, "\nAliases found: %i\n\n", aliasCount);
        for (int i = 0; i < aliasCount; i++)
        {
            fprintf(outFile, "Alias %03i: %s\n", i + 1, aliases[i].name);
            fprintf(outFile, "  Type: %s\n", aliases[i].type);
            fprintf(outFile, "  Name: %s\n", aliases[i].name);
            fprintf(outFile, "  Description: %s\n", aliases[i].desc);
        }

        // Print enums info
        fprintf(outFile, "\nEnums found: %i\n\n", enumCount);
        for (int i = 0; i < enumCount; i++)
        {
            fprintf(outFile, "Enum %02i: %s (%i values)\n", i + 1, enums[i].name, enums[i].valueCount);
            fprintf(outFile, "  Name: %s\n", enums[i].name);
            fprintf(outFile, "  Description: %s\n", enums[i].desc);
            for (int e = 0; e < enums[i].valueCount; e++)
                fprintf(outFile, "  Value[%s]: %i\n", enums[i].valueName[e], enums[i].valueInteger[e]);
        }

        // Print callbacks info
        fprintf(outFile, "\nCallbacks found: %i\n\n", callbackCount);
        for (int i = 0; i < callbackCount; i++)
        {
            fprintf(outFile, "Callback %03i: %s() (%i input parameters)\n", i + 1, callbacks[i].name, callbacks[i].paramCount);
            fprintf(outFile, "  Name: %s\n", callbacks[i].name);
            fprintf(outFile, "  Return type: %s\n", callbacks[i].retType);
            fprintf(outFile, "  Description: %s\n", callbacks[i].desc);
            for (int p = 0; p < callbacks[i].paramCount; p++)
                fprintf(outFile, "  Param[%i]: %s (type: %s)\n", p + 1, callbacks[i].paramName[p], callbacks[i].paramType[p]);
            if (callbacks[i].paramCount == 0)
                fprintf(outFile, "  No input parameters\n");
        }

        // Print functions info
        fprintf(outFile, "\nFunctions found: %i\n\n", funcCount);
        for (int i = 0; i < funcCount; i++)
        {
            fprintf(outFile, "Function %03i: %s() (%i input parameters)\n", i + 1, funcs[i].name, funcs[i].paramCount);
            fprintf(outFile, "  Name: %s\n", funcs[i].name);
            fprintf(outFile, "  Return type: %s\n", funcs[i].retType);
            fprintf(outFile, "  Description: %s\n", funcs[i].desc);
            for (int p = 0; p < funcs[i].paramCount; p++)
                fprintf(outFile, "  Param[%i]: %s (type: %s)\n", p + 1, funcs[i].paramName[p], funcs[i].paramType[p]);
            if (funcs[i].paramCount == 0)
                fprintf(outFile, "  No input parameters\n");
        }
    }
    break;
    case JSON:
    {
        fprintf(outFile, "{\n");

        // Print defines info
        fprintf(outFile, "  \"defines\": [\n");
        for (int i = 0; i < defineCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      \"name\": \"%s\",\n", defines[i].name);
            fprintf(outFile, "      \"type\": \"%s\",\n", StrDefineType(defines[i].type));
            if (defines[i].isHex) // INT or LONG
            {
                fprintf(outFile, "      \"value\": %ld,\n", strtol(defines[i].value, NULL, 16));
            }
            else if ((defines[i].type == INT) ||
                     (defines[i].type == LONG) ||
                     (defines[i].type == FLOAT) ||
                     (defines[i].type == DOUBLE) ||
                     (defines[i].type == STRING))
            {
                fprintf(outFile, "      \"value\": %s,\n", defines[i].value);
            }
            else
            {
                fprintf(outFile, "      \"value\": \"%s\",\n", defines[i].value);
            }
            fprintf(outFile, "      \"description\": \"%s\"\n", defines[i].desc);
            fprintf(outFile, "    }");

            if (i < defineCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  ],\n");

        // Print structs info
        fprintf(outFile, "  \"structs\": [\n");
        for (int i = 0; i < structCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      \"name\": \"%s\",\n", structs[i].name);
            fprintf(outFile, "      \"description\": \"%s\",\n", EscapeBackslashes(structs[i].desc));
            fprintf(outFile, "      \"fields\": [\n");
            for (int f = 0; f < structs[i].fieldCount; f++)
            {
                fprintf(outFile, "        {\n");
                fprintf(outFile, "          \"type\": \"%s\",\n", structs[i].fieldType[f]);
                fprintf(outFile, "          \"name\": \"%s\",\n", structs[i].fieldName[f]);
                fprintf(outFile, "          \"description\": \"%s\"\n", EscapeBackslashes(structs[i].fieldDesc[f]));
                fprintf(outFile, "        }");
                if (f < structs[i].fieldCount - 1)
                    fprintf(outFile, ",\n");
                else
                    fprintf(outFile, "\n");
            }
            fprintf(outFile, "      ]\n");
            fprintf(outFile, "    }");
            if (i < structCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  ],\n");

        // Print aliases info
        fprintf(outFile, "  \"aliases\": [\n");
        for (int i = 0; i < aliasCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      \"type\": \"%s\",\n", aliases[i].type);
            fprintf(outFile, "      \"name\": \"%s\",\n", aliases[i].name);
            fprintf(outFile, "      \"description\": \"%s\"\n", aliases[i].desc);
            fprintf(outFile, "    }");

            if (i < aliasCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  ],\n");

        // Print enums info
        fprintf(outFile, "  \"enums\": [\n");
        for (int i = 0; i < enumCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      \"name\": \"%s\",\n", enums[i].name);
            fprintf(outFile, "      \"description\": \"%s\",\n", EscapeBackslashes(enums[i].desc));
            fprintf(outFile, "      \"values\": [\n");
            for (int e = 0; e < enums[i].valueCount; e++)
            {
                fprintf(outFile, "        {\n");
                fprintf(outFile, "          \"name\": \"%s\",\n", enums[i].valueName[e]);
                fprintf(outFile, "          \"value\": %i,\n", enums[i].valueInteger[e]);
                fprintf(outFile, "          \"description\": \"%s\"\n", EscapeBackslashes(enums[i].valueDesc[e]));
                fprintf(outFile, "        }");
                if (e < enums[i].valueCount - 1)
                    fprintf(outFile, ",\n");
                else
                    fprintf(outFile, "\n");
            }
            fprintf(outFile, "      ]\n");
            fprintf(outFile, "    }");
            if (i < enumCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  ],\n");

        // Print callbacks info
        fprintf(outFile, "  \"callbacks\": [\n");
        for (int i = 0; i < callbackCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      \"name\": \"%s\",\n", callbacks[i].name);
            fprintf(outFile, "      \"description\": \"%s\",\n", EscapeBackslashes(callbacks[i].desc));
            fprintf(outFile, "      \"returnType\": \"%s\"", callbacks[i].retType);

            if (callbacks[i].paramCount == 0)
                fprintf(outFile, "\n");
            else
            {
                fprintf(outFile, ",\n      \"params\": [\n");
                for (int p = 0; p < callbacks[i].paramCount; p++)
                {
                    fprintf(outFile, "        {\n");
                    fprintf(outFile, "          \"type\": \"%s\",\n", callbacks[i].paramType[p]);
                    fprintf(outFile, "          \"name\": \"%s\"\n", callbacks[i].paramName[p]);
                    fprintf(outFile, "        }");
                    if (p < callbacks[i].paramCount - 1)
                        fprintf(outFile, ",\n");
                    else
                        fprintf(outFile, "\n");
                }
                fprintf(outFile, "      ]\n");
            }
            fprintf(outFile, "    }");

            if (i < callbackCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  ],\n");

        // Print functions info
        fprintf(outFile, "  \"functions\": [\n");
        for (int i = 0; i < funcCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      \"name\": \"%s\",\n", funcs[i].name);
            fprintf(outFile, "      \"description\": \"%s\",\n", EscapeBackslashes(funcs[i].desc));
            fprintf(outFile, "      \"returnType\": \"%s\"", funcs[i].retType);

            if (funcs[i].paramCount == 0)
                fprintf(outFile, "\n");
            else
            {
                fprintf(outFile, ",\n      \"params\": [\n");
                for (int p = 0; p < funcs[i].paramCount; p++)
                {
                    fprintf(outFile, "        {\n");
                    fprintf(outFile, "          \"type\": \"%s\",\n", funcs[i].paramType[p]);
                    fprintf(outFile, "          \"name\": \"%s\"\n", funcs[i].paramName[p]);
                    fprintf(outFile, "        }");
                    if (p < funcs[i].paramCount - 1)
                        fprintf(outFile, ",\n");
                    else
                        fprintf(outFile, "\n");
                }
                fprintf(outFile, "      ]\n");
            }
            fprintf(outFile, "    }");

            if (i < funcCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  ]\n");
        fprintf(outFile, "}\n");
    }
    break;
    case XML:
    {
        // XML format to export data:
        /*
        <?xml version="1.0" encoding="Windows-1252" ?>
        <raylibAPI>
            <Defines count="">
                <Define name="" type="" value="" desc="" />
            </Defines>
            <Structs count="">
                <Struct name="" fieldCount="" desc="">
                    <Field type="" name="" desc="" />
                    <Field type="" name="" desc="" />
                </Struct>
            <Structs>
            <Aliases count="">
                <Alias type="" name="" desc="" />
            </Aliases>
            <Enums count="">
                <Enum name="" valueCount="" desc="">
                    <Value name="" integer="" desc="" />
                    <Value name="" integer="" desc="" />
                </Enum>
            </Enums>
            <Callbacks count="">
                <Callback name="" retType="" paramCount="" desc="">
                    <Param type="" name="" desc="" />
                    <Param type="" name="" desc="" />
                </Callback>
            </Callbacks>
            <Functions count="">
                <Function name="" retType="" paramCount="" desc="">
                    <Param type="" name="" desc="" />
                    <Param type="" name="" desc="" />
                </Function>
            </Functions>
        </raylibAPI>
        */

        fprintf(outFile, "<?xml version=\"1.0\" encoding=\"Windows-1252\" ?>\n");
        fprintf(outFile, "<raylibAPI>\n");

        // Print defines info
        fprintf(outFile, "    <Defines count=\"%i\">\n", defineCount);
        for (int i = 0; i < defineCount; i++)
        {
            fprintf(outFile, "        <Define name=\"%s\" type=\"%s\" ", defines[i].name, StrDefineType(defines[i].type));
            if (defines[i].type == STRING)
            {
                fprintf(outFile, "value=%s", defines[i].value);
            }
            else
            {
                fprintf(outFile, "value=\"%s\"", defines[i].value);
            }
            fprintf(outFile, " desc=\"%s\" />\n", defines[i].desc);
        }
        fprintf(outFile, "    </Defines>\n");

        // Print structs info
        fprintf(outFile, "    <Structs count=\"%i\">\n", structCount);
        for (int i = 0; i < structCount; i++)
        {
            fprintf(outFile, "        <Struct name=\"%s\" fieldCount=\"%i\" desc=\"%s\">\n", structs[i].name, structs[i].fieldCount, structs[i].desc);
            for (int f = 0; f < structs[i].fieldCount; f++)
            {
                fprintf(outFile, "            <Field type=\"%s\" name=\"%s\" desc=\"%s\" />\n", structs[i].fieldType[f], structs[i].fieldName[f], structs[i].fieldDesc[f]);
            }
            fprintf(outFile, "        </Struct>\n");
        }
        fprintf(outFile, "    </Structs>\n");

        // Print aliases info
        fprintf(outFile, "    <Aliases count=\"%i\">\n", aliasCount);
        for (int i = 0; i < aliasCount; i++)
        {
            fprintf(outFile, "        <Alias type=\"%s\" name=\"%s\" desc=\"%s\" />\n", aliases[i].name, aliases[i].type, aliases[i].desc);
        }
        fprintf(outFile, "    </Aliases>\n");

        // Print enums info
        fprintf(outFile, "    <Enums count=\"%i\">\n", enumCount);
        for (int i = 0; i < enumCount; i++)
        {
            fprintf(outFile, "        <Enum name=\"%s\" valueCount=\"%i\" desc=\"%s\">\n", enums[i].name, enums[i].valueCount, enums[i].desc);
            for (int v = 0; v < enums[i].valueCount; v++)
            {
                fprintf(outFile, "            <Value name=\"%s\" integer=\"%i\" desc=\"%s\" />\n", enums[i].valueName[v], enums[i].valueInteger[v], enums[i].valueDesc[v]);
            }
            fprintf(outFile, "        </Enum>\n");
        }
        fprintf(outFile, "    </Enums>\n");

        // Print callbacks info
        fprintf(outFile, "    <Callbacks count=\"%i\">\n", callbackCount);
        for (int i = 0; i < callbackCount; i++)
        {
            fprintf(outFile, "        <Callback name=\"%s\" retType=\"%s\" paramCount=\"%i\" desc=\"%s\">\n", callbacks[i].name, callbacks[i].retType, callbacks[i].paramCount, callbacks[i].desc);
            for (int p = 0; p < callbacks[i].paramCount; p++)
            {
                fprintf(outFile, "            <Param type=\"%s\" name=\"%s\" desc=\"%s\" />\n", callbacks[i].paramType[p], callbacks[i].paramName[p], callbacks[i].paramDesc[p]);
            }
            fprintf(outFile, "        </Callback>\n");
        }
        fprintf(outFile, "    </Callbacks>\n");

        // Print functions info
        fprintf(outFile, "    <Functions count=\"%i\">\n", funcCount);
        for (int i = 0; i < funcCount; i++)
        {
            fprintf(outFile, "        <Function name=\"%s\" retType=\"%s\" paramCount=\"%i\" desc=\"%s\">\n", funcs[i].name, funcs[i].retType, funcs[i].paramCount, funcs[i].desc);
            for (int p = 0; p < funcs[i].paramCount; p++)
            {
                fprintf(outFile, "            <Param type=\"%s\" name=\"%s\" desc=\"%s\" />\n", funcs[i].paramType[p], funcs[i].paramName[p], funcs[i].paramDesc[p]);
            }
            fprintf(outFile, "        </Function>\n");
        }
        fprintf(outFile, "    </Functions>\n");

        fprintf(outFile, "</raylibAPI>\n");
    }
    break;
    case LUA:
    {
        fprintf(outFile, "return {\n");

        // Print defines info
        fprintf(outFile, "  defines = {\n");
        for (int i = 0; i < defineCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      name = \"%s\",\n", defines[i].name);
            fprintf(outFile, "      type = \"%s\",\n", StrDefineType(defines[i].type));
            if ((defines[i].type == INT) ||
                (defines[i].type == LONG) ||
                (defines[i].type == FLOAT) ||
                (defines[i].type == DOUBLE) ||
                (defines[i].type == STRING))
            {
                fprintf(outFile, "      value = %s,\n", defines[i].value);
            }
            else
            {
                fprintf(outFile, "      value = \"%s\",\n", defines[i].value);
            }
            fprintf(outFile, "      description = \"%s\"\n", defines[i].desc);
            fprintf(outFile, "    }");

            if (i < defineCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  },\n");

        // Print structs info
        fprintf(outFile, "  structs = {\n");
        for (int i = 0; i < structCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      name = \"%s\",\n", structs[i].name);
            fprintf(outFile, "      description = \"%s\",\n", EscapeBackslashes(structs[i].desc));
            fprintf(outFile, "      fields = {\n");
            for (int f = 0; f < structs[i].fieldCount; f++)
            {
                fprintf(outFile, "        {\n");
                fprintf(outFile, "          type = \"%s\",\n", structs[i].fieldType[f]);
                fprintf(outFile, "          name = \"%s\",\n", structs[i].fieldName[f]);
                fprintf(outFile, "          description = \"%s\"\n", EscapeBackslashes(structs[i].fieldDesc[f]));
                fprintf(outFile, "        }");
                if (f < structs[i].fieldCount - 1)
                    fprintf(outFile, ",\n");
                else
                    fprintf(outFile, "\n");
            }
            fprintf(outFile, "      }\n");
            fprintf(outFile, "    }");
            if (i < structCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  },\n");

        // Print aliases info
        fprintf(outFile, "  aliases = {\n");
        for (int i = 0; i < aliasCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      type = \"%s\",\n", aliases[i].type);
            fprintf(outFile, "      name = \"%s\",\n", aliases[i].name);
            fprintf(outFile, "      description = \"%s\"\n", aliases[i].desc);
            fprintf(outFile, "    }");

            if (i < aliasCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  },\n");

        // Print enums info
        fprintf(outFile, "  enums = {\n");
        for (int i = 0; i < enumCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      name = \"%s\",\n", enums[i].name);
            fprintf(outFile, "      description = \"%s\",\n", EscapeBackslashes(enums[i].desc));
            fprintf(outFile, "      values = {\n");
            for (int e = 0; e < enums[i].valueCount; e++)
            {
                fprintf(outFile, "        {\n");
                fprintf(outFile, "          name = \"%s\",\n", enums[i].valueName[e]);
                fprintf(outFile, "          value = %i,\n", enums[i].valueInteger[e]);
                fprintf(outFile, "          description = \"%s\"\n", EscapeBackslashes(enums[i].valueDesc[e]));
                fprintf(outFile, "        }");
                if (e < enums[i].valueCount - 1)
                    fprintf(outFile, ",\n");
                else
                    fprintf(outFile, "\n");
            }
            fprintf(outFile, "      }\n");
            fprintf(outFile, "    }");
            if (i < enumCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  },\n");

        // Print callbacks info
        fprintf(outFile, "  callbacks = {\n");
        for (int i = 0; i < callbackCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      name = \"%s\",\n", callbacks[i].name);
            fprintf(outFile, "      description = \"%s\",\n", EscapeBackslashes(callbacks[i].desc));
            fprintf(outFile, "      returnType = \"%s\"", callbacks[i].retType);

            if (callbacks[i].paramCount == 0)
                fprintf(outFile, "\n");
            else
            {
                fprintf(outFile, ",\n      params = {\n");
                for (int p = 0; p < callbacks[i].paramCount; p++)
                {
                    fprintf(outFile, "        {type = \"%s\", name = \"%s\"}", callbacks[i].paramType[p], callbacks[i].paramName[p]);
                    if (p < callbacks[i].paramCount - 1)
                        fprintf(outFile, ",\n");
                    else
                        fprintf(outFile, "\n");
                }
                fprintf(outFile, "      }\n");
            }
            fprintf(outFile, "    }");

            if (i < callbackCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  },\n");

        // Print functions info
        fprintf(outFile, "  functions = {\n");
        for (int i = 0; i < funcCount; i++)
        {
            fprintf(outFile, "    {\n");
            fprintf(outFile, "      name = \"%s\",\n", funcs[i].name);
            fprintf(outFile, "      description = \"%s\",\n", EscapeBackslashes(funcs[i].desc));
            fprintf(outFile, "      returnType = \"%s\"", funcs[i].retType);

            if (funcs[i].paramCount == 0)
                fprintf(outFile, "\n");
            else
            {
                fprintf(outFile, ",\n      params = {\n");
                for (int p = 0; p < funcs[i].paramCount; p++)
                {
                    fprintf(outFile, "        {type = \"%s\", name = \"%s\"}", funcs[i].paramType[p], funcs[i].paramName[p]);
                    if (p < funcs[i].paramCount - 1)
                        fprintf(outFile, ",\n");
                    else
                        fprintf(outFile, "\n");
                }
                fprintf(outFile, "      }\n");
            }
            fprintf(outFile, "    }");

            if (i < funcCount - 1)
                fprintf(outFile, ",\n");
            else
                fprintf(outFile, "\n");
        }
        fprintf(outFile, "  }\n");
        fprintf(outFile, "}\n");
    }
    break;
    case CODE:
    default:
        break;
    }

    fclose(outFile);
}
