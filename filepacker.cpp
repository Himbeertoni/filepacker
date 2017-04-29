#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <cstring>
#include <assert.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned __int64 u64;

#define RP_ASSERT(x) assert(x)

inline
u32 stringLength(char const * S, u32 maxLen=-1)
{
    u32 result = 0;
    while(*(S++) && result++ <= maxLen);
    return result;
}

inline
bool stringEqual(char const * A, char const * B)
{
    bool result = false;
    if(stringLength(A) == stringLength(B))
    {
        result = true;
        while(*A && result)
        {
            result = *(A++) == *(B++);
        }
    }
    return result;
}

inline
void stringCopy(char* D, char const* S, u32 maxLen, u32 charCount)
{
    RP_ASSERT(stringLength(S) < maxLen);
    RP_ASSERT(charCount <= maxLen);
    u32 minCount = maxLen < charCount ? maxLen : charCount;
    u32 counter = 0;
    while(true)
    {
        *(D++) = *S;
        if(!(*S++ && counter++ < minCount))
            break; 
    }
}

inline
void stringCat(char * D, char const * S, u32 maxLen)
{
    RP_ASSERT(stringLength(S) + stringLength(D) < maxLen);
    u32 counter = stringLength(D);
    char* at = D + counter;
    stringCopy(at, S, maxLen, maxLen); 
}

inline
bool stringEndsIn(char const * A, char const * B)
{
    u32 comparandLen = stringLength(A);
    u32 patternLen = stringLength(B);
    if(comparandLen >= patternLen)
    {
        return stringEqual(A + (comparandLen - patternLen), B);
    }
    return false;
} 

inline
bool stringStartsWith(char const * A, char const * B)
{
    bool result = false;
    while(*A && *B && (*A == *B))
    {
        ++A;
        ++B;
    }
    if(!*B)
    {
        result = true;
    }
    return result;
}

inline
u32 stringFindSubstring(char const* A, char const * S)
{
    u32 result = -1;
    u32 counter = 0;
    while(*(A + counter))
    {
        char const * B = (A + counter);
        if(stringStartsWith(B, S))
        {
            result = counter;
            break;
        }
        ++counter;
    }
    return result;
}

enum FileType
{
    FT_INVALID = 0,
    FT_ANY
    //TODO(alg): filter by type
};

struct FileEntry
{
    char name[MAX_PATH];
    char path[MAX_PATH];
    u32 nameLen;
    u32 pathLen;
    u64 size;
    FileType type;
};

u32 const MAGIC = 0xDEADBEEF;

FileEntry fileEntries[4096];
u64 fileOffsets[4096];
u32 fileEntryCount;

static
void findFilesRecursively(char const * basePath, char const * subPath, FileEntry* files, u32* fileCount)
{
    u32 subPathLen = stringLength(subPath);
    if(stringLength(basePath) + subPathLen + (subPathLen > 0 ? 1 : 0) >= MAX_PATH)
    {
        printf("Error: path too long\n");
        return;
    }
    
    char absolutePath[MAX_PATH] = {};
    stringCopy(absolutePath, basePath, MAX_PATH, MAX_PATH);
    if(subPathLen > 0)
    {
        stringCat(absolutePath, "/", MAX_PATH);
        stringCat(absolutePath, subPath, MAX_PATH);
    }
    
    u32 absolutePathLen = stringLength(absolutePath, MAX_PATH);
    
    if (absolutePathLen < (MAX_PATH - 3))
    {    
        //Extend directoy name string with \* so it directly finds all files/dirs under root
        stringCat(absolutePath, "\\*", MAX_PATH);
        
        WIN32_FIND_DATA findData;
        HANDLE findHandle = FindFirstFileA(absolutePath, &findData);
        if(findHandle == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            printf("Could not find %s (%d)\n", absolutePath, err);
        }
        else
        {
            do
            {
                if(!stringEqual(findData.cFileName, ".") && !stringEqual(findData.cFileName, ".."))
                {
                    char relPath[MAX_PATH]  = {};
                    if(subPathLen + stringLength(findData.cFileName) + (subPathLen > 0 ? 1 : 0) < MAX_PATH)
                    {
                        stringCopy(relPath, subPath, MAX_PATH, MAX_PATH);
                        if(subPathLen > 0)
                        {
                            stringCat(relPath, "/", MAX_PATH);
                        }
                        stringCat(relPath, findData.cFileName, MAX_PATH);
                        
                        if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        {
                            findFilesRecursively(basePath, relPath, files, fileCount);
                        }
                        else
                        {
                            FileType fileType = FT_ANY;
                            #if 0 //TODO(alg): filter by file ending
                            if(stringEndsIn(findData.cFileName, ".h"))
                            {
                                fileType = FT_HEADER;
                            }
                            #endif
                            
                            if(fileType == FT_ANY)
                            {                          
                                LARGE_INTEGER fileSize;
                                fileSize.LowPart = findData.nFileSizeLow;
                                fileSize.HighPart = findData.nFileSizeHigh;
                                
                                FileEntry newEntry = {};
                                stringCopy(newEntry.path, relPath, MAX_PATH, MAX_PATH);
                                newEntry.pathLen = stringLength(relPath) + 1; //NOTE(alg): null-termination
                                stringCopy(newEntry.name, findData.cFileName, MAX_PATH, MAX_PATH);
                                newEntry.nameLen = stringLength(findData.cFileName) + 1; //NOTE(alg): null-termination;
                                newEntry.size = fileSize.QuadPart;
                                newEntry.type = fileType;
                                
                                files[(*fileCount)++] = newEntry;
                            }
                        }
                    }
                }
            } while(FindNextFile(findHandle, &findData) != 0);
            
            DWORD err = GetLastError();
            
            FindClose(findHandle);   
        }
    }
    else
    {
        printf("\nDirectory path is too long.\n");
    }    
}

static
bool packIntoBufferAndWriteFile(char const * basePath, char const * packFileName)
{
    bool result = true;
    
    // NOTE(alg): file header
    
    // Any client always reads the complete header, can therefore build table and do random access to packed file.
    // Optionally, can read whole file at once.
    
    //File Format:
    // 1. Magic number (identifies file as packed file): 4 bytes
    // 2. Version (for backwards-compatibility): 4 bytes
    // 3. Header Size (number of bytes from file start to beginning of actual file data): 4 bytes
    // For each file:
    //    4. type: 4 bytes
    //    5. nameLength (includes null-terminator): 4 bytes
    //    6. name (ANSI, null-terminated): <nameLength> bytes
    //    7. pathLength (includes null-terminator): 4 bytes
    //    8. path (ANSI, null-terminated): <pathLength> bytes
    //    9. offset (where actual file data starts within the file): 8 bytes
    //  10. size (of actual file data, i.e. the original file size, excluding null-terminator): 8 bytes
    // 11. Actual file data, tightly packed
    
    u64 packFileHeaderSize = sizeof(u32) + sizeof(u32) + sizeof(u32);
    for(u32 i=0; i<fileEntryCount; ++i)
    {
        FileEntry* entry = fileEntries + i;
        packFileHeaderSize += sizeof(u32) + sizeof(u32)  + entry->nameLen
            + sizeof(u32) + entry->pathLen + sizeof(u64) + sizeof(u64);
    }
    
    u64 sizeOffset = packFileHeaderSize;
    for(u32 i=0; i<fileEntryCount; ++i)
    {
        fileOffsets[i] = sizeOffset;
        sizeOffset += fileEntries[i].size + 1 /*+null-terminator*/;
    }

    u64 totalBufferSize = sizeOffset;
    void* filesBuffer = malloc(totalBufferSize);
    
    void* fileHeader = filesBuffer;
    u32 version = 0;
    u64 offset = 0;
    memcpy((char*)fileHeader + offset, &MAGIC, sizeof(u32));
    offset += sizeof(u32);
    memcpy((char*)fileHeader + offset, &version, sizeof(u32)); 
    offset += sizeof(u32);
    memcpy((char*)fileHeader + offset, &packFileHeaderSize, sizeof(u32));
    offset += sizeof(u32);
    
    for(u32 i=0; i<fileEntryCount; ++i)
    {
        FileEntry* entry = fileEntries + i;
        
        u32 fileType = (u32)entry->type;
        memcpy((char*)fileHeader + offset, &fileType, sizeof(u32));
        offset += sizeof(u32);
        memcpy((char*)fileHeader + offset, &entry->nameLen, sizeof(u32));
        offset += sizeof(u32);
        memcpy((char*)fileHeader + offset, entry->name, entry->nameLen);
        offset += entry->nameLen;
        memcpy((char*)fileHeader + offset, &entry->pathLen, sizeof(u32));
        offset += sizeof(u32);
        memcpy((char*)fileHeader + offset, entry->path, entry->pathLen);
        offset += entry->pathLen;
        memcpy((char*)fileHeader + offset, &entry->size, sizeof(u64));
        offset += sizeof(u64);
        memcpy((char*)fileHeader + offset, &fileOffsets[i], sizeof(u64));
        offset += sizeof(u64);
    }
    RP_ASSERT(offset == packFileHeaderSize);
    
    for(u32 i=0; i<fileEntryCount; ++i)
    {
        FileEntry* entry = fileEntries + i;
        printf("%s %ld bytes\n", entry->name, entry->size);
        {
            char absolutePath[MAX_PATH] = {};
            stringCopy(absolutePath, basePath, MAX_PATH, MAX_PATH);
            stringCat(absolutePath, "/", MAX_PATH);
            stringCat(absolutePath, entry->path, MAX_PATH);
            
            HANDLE file = CreateFile(absolutePath, 
                                     GENERIC_READ,
                                     FILE_SHARE_READ,
                                     NULL,
                                     OPEN_EXISTING,
                                     NULL,
                                     NULL);
            if(file != INVALID_HANDLE_VALUE)
            {
                void* fileData = (char*)filesBuffer + fileOffsets[i];
                
                DWORD readByteCount = 0;
                BOOL success = ReadFile(file, fileData, entry->size, &readByteCount, NULL);
                if(success == TRUE)
                {
                    RP_ASSERT(readByteCount == entry->size);
                    char* fileStr = (char*)fileData;
                    //NOTE(alg): do sth with file contents here, e.g. obfuscate/cipher the contents
                    fileStr[entry->size] = 0; //NOTE(alg): null-terminate
                }
                else
                {
                    printf("Error reading file %s\n", entry->name);
                    result  = false;
                }
                CloseHandle(file);
            }
            else
            {
                printf("Error creating file %s\n", entry->name);
                result = false;
            }
        }
    }
    
    int ri = (int)strlen(packFileName);
    while(packFileName[ri] != '\\' && packFileName[ri] != '/' && ri > 0) --ri;
    char packFileDir[MAX_PATH] = {};
    memcpy(packFileDir, packFileName, ri);
    BOOL createDirResult = CreateDirectory(packFileDir, NULL);
    HANDLE outputFile = CreateFile(packFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(outputFile == INVALID_HANDLE_VALUE)
    {
        printf("Error: Could not create file %s\n", packFileName);
        result = false;
    }
    else
    {
        DWORD writtenByteCount = 0;
        BOOL writeSuccess = WriteFile(outputFile, filesBuffer, totalBufferSize, &writtenByteCount, NULL);
        if(writeSuccess == TRUE)
        {
            RP_ASSERT(totalBufferSize == writtenByteCount);
        }
        else
        {
            printf("Error: could not write file %s\n", packFileName);
            result = false;
        }
        CloseHandle(outputFile);
    }
    free(filesBuffer);
    return result;
}

static
void createDirectoriesRecursively(char const * pathToFile)
{
    bool delimiterFound = false;
    u32 lastAt = -1;
    do
    {
        u32 at = stringFindSubstring(lastAt + 1  + pathToFile, "/");
        if(at == -1)
        {
            at = stringFindSubstring(lastAt + 1 + pathToFile, "\\");
        }
        delimiterFound = at != -1 && at > 0;  //NOTE(alg): path starting with "\\" is not allowed -> no network paths!
        if(delimiterFound)
        {
            char dir[MAX_PATH] = {};
            stringCopy(dir, pathToFile, MAX_PATH, lastAt + at);
            BOOL success = CreateDirectory(dir, NULL);
            if(success == TRUE)
            {
                //printf("SUCCESS: %s created.\n", dir);
            }
            else
            {
                DWORD err = GetLastError();
                if(err == ERROR_ALREADY_EXISTS)
                {
                    //printf("WARNING: %s already exists!\n", dir);
                }
                else
                {
                    printf("ERROR: while creating %s\n", dir);
                }
            }
        }
        lastAt += at + 1;
    } while(delimiterFound);
}

//NOTE(alg): an extractor could choose to read only the first 12 bytes to know the header size
//then it could only read the header, and read the files from disk on demand!

void readFileAndExtractToDisk(char const * packFilePath, char const * targetDir)
{   
    WIN32_FIND_DATA findData;
    HANDLE findHandle = FindFirstFileA(packFilePath, &findData);
    if(findHandle != INVALID_HANDLE_VALUE)
    {
        HANDLE inputFile = CreateFile(packFilePath, 
                                      GENERIC_READ,
                                      FILE_SHARE_READ,
                                      NULL,
                                      OPEN_EXISTING,
                                      NULL,
                                      NULL);
        if(inputFile != INVALID_HANDLE_VALUE)
        { 
            LARGE_INTEGER inputFileSize;
            inputFileSize.LowPart = findData.nFileSizeLow;
            inputFileSize.HighPart = findData.nFileSizeHigh;
            
            void* inputFileBuffer = malloc(inputFileSize.QuadPart);
            if(inputFileBuffer)
            {
                DWORD readByteCount = 0;
                BOOL success = ReadFile(inputFile, inputFileBuffer, inputFileSize.QuadPart, &readByteCount, NULL);
                if(success  == TRUE)
                {
                    RP_ASSERT(inputFileSize.QuadPart == readByteCount);
                    u64 offset = 0;
                    u32 magic = 0;
                    memcpy(&magic,  (char*)inputFileBuffer + offset, sizeof(u32));
                    offset += sizeof(u32);
                    RP_ASSERT(magic == MAGIC);
                    u32 version = 0;
                    memcpy(&version, (char*)inputFileBuffer + offset, sizeof(u32));
                    offset += sizeof(32);
                    RP_ASSERT(version == 0);
                    u32 headerSize = 0;
                    memcpy(&headerSize, (char*)inputFileBuffer + offset, sizeof(u32));
                    offset += sizeof(u32);
                    while(offset < headerSize)
                    {
                        FileEntry* entry = fileEntries + fileEntryCount;
                        
                        u32 fileType = 0;
                        memcpy(&fileType, (char*)inputFileBuffer + offset, sizeof(u32));
                        offset += sizeof(u32);
                        entry->type = (FileType)fileType;
                        memcpy(&entry->nameLen, (char*)inputFileBuffer + offset, sizeof(u32));
                        offset += sizeof(u32);
                        memcpy(entry->name, (char*)inputFileBuffer + offset, entry->nameLen);
                        offset += entry->nameLen;
                        memcpy(&entry->pathLen, (char*)inputFileBuffer + offset, sizeof(u32));
                        offset += sizeof(u32);
                        memcpy(entry->path, (char*)inputFileBuffer + offset, entry->pathLen);
                        offset += entry->pathLen;
                        memcpy(&entry->size, (char*)inputFileBuffer + offset, sizeof(u64));
                        offset += sizeof(u64);
                        memcpy(&fileOffsets[fileEntryCount], (char*)inputFileBuffer + offset, sizeof(u64));
                        offset += sizeof(u64);
                        ++fileEntryCount;
                    }
                    
                    for(u32 i=0; i<fileEntryCount; ++i)
                    {
                        FileEntry* entry = fileEntries + i;
                        printf("%s %ld bytes\n", entry->name, entry->size);
                        
                        void* fileContents = (char*)inputFileBuffer + fileOffsets[i];
                        //NOTE(alg): do sth with file contents here, e.g. de-obfuscate
                        
                        char buffer[MAX_PATH] = {};
                        stringCopy(buffer, targetDir, MAX_PATH, MAX_PATH);
                        stringCat(buffer, "/", MAX_PATH);
                        stringCat(buffer, entry->path, MAX_PATH);
                        
                        createDirectoriesRecursively(buffer);
                        
                        HANDLE outputFile = CreateFile(buffer, 
                                                       GENERIC_WRITE, 
                                                       FILE_SHARE_READ, 
                                                       NULL, 
                                                       CREATE_ALWAYS, 
                                                       FILE_ATTRIBUTE_NORMAL, 
                                                       NULL);
                        if(outputFile != INVALID_HANDLE_VALUE)
                        {
                            DWORD writtenByteCount = 0;
                            BOOL writeSuccess = WriteFile(outputFile, fileContents, entry->size, &writtenByteCount, NULL); 
                            RP_ASSERT(entry->size == writtenByteCount);
                            if(writeSuccess != TRUE)
                            {
                                printf("Error: could not write file %s\n", buffer);
                            }
                            CloseHandle(outputFile);
                        }
                        else
                        {
                            printf("Error creating file %s\n", entry->name);
                        }
                        
                    }    
                }
                else
                {
                    printf("Error: Could not read file %s\n", packFilePath);
                }
            }
        }
        else
        {
            printf("Error: cannot create file  %s\n", packFilePath); 
        }
        FindClose(findHandle);
    }
    else
    {
        DWORD err = GetLastError();
        printf("Could not find %s (%d)\n", packFilePath, err);
    }
}

#if defined PACKER

int main(int argc, const char* argv[])
{
    fileEntryCount = 0;
    
    if(argc < 3)
    {
        printf("Usage: filepacker <path-to-source-directory> <path-to-target-file>\n");
        printf("Example: filepacker C:/myDir C:/packedMyDir.bin\n");
        return -1;
    }
    //NOTE(alg): may not contain trailing backslash!!
    char const* sourceDirPath = argv[1];
    
    findFilesRecursively(sourceDirPath, "", fileEntries, &fileEntryCount);
    
    char const* targetFilePath = argv[2];
    bool result = packIntoBufferAndWriteFile(sourceDirPath, targetFilePath);
    return result ? 0 : -1;
}

#elif defined UNPACKER

int main(int argc, const char* argv[])
{
    fileEntryCount = 0;
    
    if(argc < 3)
    {
        printf("Usage: fileunpacker <path-to-packed-file> <path-to-target-directory>\n");
        printf("Example: fileunpacker C:/packedMyDir.bin C:/unpackedMyDir\n");
        return -1;
    }
    char const* packfilename = argv[1];
    //NOTE(alg): must point to an existing directory!
    //NOTE(alg): may not contain trailing backslash!!
    char const * extractTargetDir = argv[2];
    readFileAndExtractToDisk(packfilename, extractTargetDir);
    return 0;
}

#elif defined FILEPACKERTEST

FileEntry filesA[4096];
FileEntry filesB[4096];
u32 fileCountA;
u32 fileCountB;

u32 findFileIn(char const * relPath, FileEntry* files, u32 fileCount)
{
    for(u32 i=0; i<fileCount; ++i)
    {
        FileEntry* f = files + i;
        if(stringEqual(relPath, f->path))
        {
            return i;
        }  
    }
    return -1;
}

static
bool compareDirectoryTreeContents(char const * A, char const * B)
{
    bool result = true;
    findFilesRecursively(A, "", filesA, &fileCountA);
    findFilesRecursively(B, "", filesB, &fileCountB);
    if(fileCountA == fileCountB)
    {
        for(u32 i=0; i<fileCountA; ++i)
        {
            FileEntry* a = filesA + i;
            
            char bufferA[MAX_PATH];
            stringCopy(bufferA, A, MAX_PATH, MAX_PATH);
            stringCat(bufferA, "/", MAX_PATH);
            stringCat(bufferA, a->path, MAX_PATH);
            
            HANDLE inputFileA = CreateFile(bufferA, 
                                           GENERIC_READ,
                                           FILE_SHARE_READ,
                                           NULL,
                                           OPEN_EXISTING,
                                           NULL,
                                           NULL);
            if(inputFileA != INVALID_HANDLE_VALUE)
            {
                u32 foundIdx = findFileIn(a->path, filesB, fileCountB);
                if(foundIdx != -1)
                {
                    FileEntry* b = filesB + foundIdx;
                    char bufferB[MAX_PATH];
                    stringCopy(bufferB, B, MAX_PATH, MAX_PATH);
                    stringCat(bufferB, "/", MAX_PATH);
                    stringCat(bufferB, b->path, MAX_PATH);   
                    
                    HANDLE inputFileB = CreateFile(bufferB, 
                                                   GENERIC_READ,
                                                   FILE_SHARE_READ,
                                                   NULL,
                                                   OPEN_EXISTING,
                                                   NULL,
                                                   NULL);
                    if(inputFileB != INVALID_HANDLE_VALUE)
                    {
                        void* inputFileBufferA = malloc(a->size);
                        void* inputFileBufferB = malloc(b->size);
                        {
                            DWORD readByteCount = 0;
                            BOOL success = ReadFile(inputFileA, inputFileBufferA, a->size, &readByteCount, NULL);
                            RP_ASSERT(readByteCount == a->size);
                        }
                        {
                            DWORD readByteCount = 0;
                            BOOL success = ReadFile(inputFileB, inputFileBufferB, b->size, &readByteCount, NULL);
                            RP_ASSERT(readByteCount == b->size);
                        }
                        bool equal = stringEqual((char*)inputFileBufferA, (char*)inputFileBufferB);
                        if(!equal)
                        {
                            printf("ERROR: files %s and %s are different\n", a->path, b->path);
                        }
                        result &= equal;
                    }
                }
            }
        }
    }
    else
    {
        printf("Error: file count different %u vs. %u\n", fileCountA, fileCountB);
        result = false;
    }
    return result;
}

int main(int argc, const char* argv[])
{
    fileEntryCount = 0;
    
    if(argc < 3)
    {
        printf("Usage: filepackertest <path-to-source-directory> <path-to-target-directory>\n");
        printf("Example: filepackertest C:/myDir C:/myTestDir\n");
        return -1;
    }
    
    //NOTE(alg): may not contain trailing backslash!!
    char const* dir = argv[1];
    findFilesRecursively(dir, "", fileEntries, &fileEntryCount);
    char const * packFileName = "packed.bin";
    packIntoBufferAndWriteFile(dir, packFileName);
    
    char const * packFilePath = "packed.bin";
    
    fileEntryCount = 0;
    //NOTE(alg): must point to an existing directory!
    //NOTE(alg): may not contain trailing backslash!!
    char const * extractTargetDir = argv[2];
    
    //TODO(alg): delete directories/files beneath extractTargetDir to rule out results from previous runs
    
    readFileAndExtractToDisk(packFilePath, extractTargetDir);
    
    bool ok = compareDirectoryTreeContents(dir, extractTargetDir);
    RP_ASSERT(ok);
    printf("Result : %s\n", ok ? "OK" : "FAIL");
    return 0;
}

#else

#error "Must define either PACKER, UNPACKER or FILEPACKERTEST to build an executable.

#endif