#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <set>
#include <iostream>

// https://github.com/dotnet/coreclr/blob/master/src/coreclr/hosts/inc/coreclrhost.h
#include "coreclrhost.h"

#if defined(_WIN32) || defined(__WIN32__)
#   define OS_WIN
#elif defined(__APPLE__)
#   define OS_OSX
#else
#   define OS_POSIX
#endif

// Define OS-specific items like the CoreCLR library's name and path elements
#if defined(OS_WIN)
#   include <Windows.h>
#   define FS_SEPARATOR "\\"
#   define PATH_DELIMITER ";"
#   define DYNLIB_LOAD(path)    LoadLibraryExA(path, 0, 0)
#   define DYNLIB_UNLOAD(a)     !FreeLibrary(a)
#   define DYNLIB_GETSYM(a,b )  GetProcAddress(a, b)
#   define FREE(x)              CoTaskMemFree(x)
#else
#   include <dirent.h>
#   include <dlfcn.h>
#   include <limits.h>
#   include <sys/stat.h>
#   define FS_SEPARATOR "/"
#   define PATH_DELIMITER ":"
#   define MAX_PATH PATH_MAX
#   define HMODULE void*
#   define DYNLIB_LOAD(path)    dlopen(path, RTLD_NOW | RTLD_LOCAL)
#   define DYNLIB_UNLOAD(a)     dlclose(a)
#   define DYNLIB_GETSYM(a,b )  dlsym(a, b)
#   define FREE(x)              free(x)
#endif

#if defined(OS_WIN)
#   define CORECLR_FILE_NAME "coreclr.dll"
#elif defined(OS_OSX)
#   define CORECLR_FILE_NAME "libcoreclr.dylib"
#else
#   define CORECLR_FILE_NAME "libcoreclr.so"
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define MANAGED_ASSEMBLY "ManagedLibrary.dll"

// Function pointer types for the managed call and callback
typedef int (*report_callback_ptr)(int progress);
typedef char* (*doWork_ptr)(const char* jobName, int iterations, int dataSize, double* data, report_callback_ptr callbackFunction);

void BuildTpaList(const char* directory, const char* extension, std::string& tpaList);
int  ReportProgressCallback(int progress);

int main(int argc, char** argv) {
    const char* core_clr_dir = "./";
    if (argc >= 2)
    {
        core_clr_dir = argv[1];
        // std::cerr << "Usage: host <core_clr_path>" << std::endl;
        // return -1;
    }

    // Get the current executable's directory
    // This sample assumes that both CoreCLR and the
    // managed assembly to be loaded are next to this host
    // so we need to get the current path in order to locate those.
    char appPath[MAX_PATH];
#if defined(OS_WIN)
    GetFullPathNameA(argv[0], MAX_PATH, appPath, NULL);
#else
    realpath(argv[0], appPath);
#endif

    char *last_slash = strrchr(appPath, FS_SEPARATOR[0]);
    if (last_slash != NULL)
        *last_slash = 0;
        
    // Construct the CoreCLR path
    // For this sample, we know CoreCLR's path. For other hosts,
    // it may be necessary to probe for coreclr.dll/libcoreclr.so
    std::string coreClrPath(core_clr_dir);
    coreClrPath.append(FS_SEPARATOR);
    coreClrPath.append(CORECLR_FILE_NAME);

    // Construct the managed library path
    std::string managedLibraryPath(appPath);
    managedLibraryPath.append(FS_SEPARATOR);
    managedLibraryPath.append(MANAGED_ASSEMBLY);

    // STEP 1: Load CoreCLR (coreclr.dll/libcoreclr.so)
    // const char* libcoreclr = "/usr/local/share/dotnet/shared/Microsoft.NETCore.App/2.0.0/libcoreclr.dylib";
    HMODULE coreClr = DYNLIB_LOAD(CORECLR_FILE_NAME);
    if (coreClr == NULL){
        printf("ERROR: Failed to load CoreCLR from %s\n", CORECLR_FILE_NAME);
        return -1;
    } else {
        printf("Loaded CoreCLR from %s\n", CORECLR_FILE_NAME);
    }

    // STEP 2: Get CoreCLR hosting functions pInitPtr pCreatePtr,pShutdownPtr
    coreclr_initialize_ptr pInitPtr = (coreclr_initialize_ptr)DYNLIB_GETSYM(coreClr, "coreclr_initialize");
    coreclr_create_delegate_ptr pCreateDelegatePtr = (coreclr_create_delegate_ptr)DYNLIB_GETSYM(coreClr, "coreclr_create_delegate");
    coreclr_shutdown_ptr pShutdownPtr = (coreclr_shutdown_ptr)DYNLIB_GETSYM(coreClr, "coreclr_shutdown");

    if (pInitPtr == NULL) {
        printf("coreclr_initialize not found");
        return -1;
    }

    if (pCreateDelegatePtr == NULL) {
        printf("coreclr_create_delegate not found");
        return -1;
    }

    if (pShutdownPtr == NULL) {
        printf("coreclr_shutdown not found");
        return -1;
    }

    // STEP 3: Construct properties used when starting the runtime

    // Construct the trusted platform assemblies (TPA) list
    // This is the list of assemblies that .NET Core can load as
    // trusted system assemblies.
    // For this host (as with most), assemblies next to CoreCLR will
    // be included in the TPA list
    std::string tpaList;
    BuildTpaList(appPath, ".dll", tpaList);

    // <Snippet3>
    // Define CoreCLR properties
    // Other properties related to assembly loading are common here,
    // but for this simple sample, TRUSTED_PLATFORM_ASSEMBLIES is all
    // that is needed. Check hosting documentation for other common properties.
    const char* propertyKeys[] = {
        "APP_PATHS",
        "TRUSTED_PLATFORM_ASSEMBLIES"      // Trusted assemblies
    };

    const char* propertyValues[] = {
        appPath,
        tpaList.c_str()
    };
    // </Snippet3>

    // STEP 4: Start the CoreCLR runtime

    // <Snippet4>
    void* hostHandle;
    unsigned int domainId;

    // This function both starts the .NET Core runtime and creates
    // the default (and only) AppDomain
    int hr = pInitPtr(
                appPath,                    // App base path
                "host",                     // AppDomain friendly name
                ARRAY_SIZE(propertyKeys),   // Property count
                propertyKeys,               // Property names
                propertyValues,             // Property values
                &hostHandle,                // Host handle
                &domainId);                 // AppDomain ID
    // </Snippet4>

    if (hr >= 0){
        printf("CoreCLR started\n");
    }else{
        printf("coreclr_initialize failed - status: 0x%08x\n", hr);
        return -1;
    }

    // STEP 5: Create delegate to managed code and invoke it

    // <Snippet5>
    doWork_ptr managedDelegate;

    // The assembly name passed in the third parameter is a managed assembly name
    // as described at https://docs.microsoft.com/dotnet/framework/app-domains/assembly-names
    hr = pCreateDelegatePtr(
            hostHandle,
            domainId,
            "ManagedLibrary, Version=1.0.0.0",
            "ManagedLibrary.ManagedWorker",
            "DoWork",
            (void**)&managedDelegate);
    // </Snippet5>

    if (hr >= 0) {
        printf("Managed delegate created\n");
    } else {
        printf("coreclr_create_delegate failed - status: 0x%08x\n", hr);
        return -1;
    }

    // Create sample data for the double[] argument of the managed method to be called
    double data[4];
    data[0] = 0;
    data[1] = 0.25;
    data[2] = 0.5;
    data[3] = 0.75;

    // Invoke the managed delegate and write the returned string to the console
    char* ret = managedDelegate("Test job", 5, sizeof(data) / sizeof(double), data, ReportProgressCallback);

    printf("Managed code returned: %s\n", ret);

    // Strings returned to native code must be freed by the native code
    FREE(ret);

    // STEP 6: Shutdown CoreCLR

    // <Snippet6>
    hr = pShutdownPtr(hostHandle, domainId);
    // </Snippet6>

    if (hr >= 0){
        printf("CoreCLR successfully shutdown\n");
    } else {
        printf("coreclr_shutdown failed - status: 0x%08x\n", hr);
    }

    // Unload CoreCLR
    if(DYNLIB_UNLOAD(coreClr)) {
        printf("Failed to free libcoreclr\n");
    }

    return 0;
}

#if defined(OS_WIN)
// Win32 directory search for .dll files
// <Snippet7>
void BuildTpaList(const char* directory, const char* extension, std::string& tpaList)
{
    // This will add all files with a .dll extension to the TPA list.
    // This will include unmanaged assemblies (coreclr.dll, for example) that don't
    // belong on the TPA list. In a real host, only managed assemblies that the host
    // expects to load should be included. Having extra unmanaged assemblies doesn't
    // cause anything to fail, though, so this function just enumerates all dll's in
    // order to keep this sample concise.
    std::string searchPath(directory);
    searchPath.append(FS_SEPARATOR);
    searchPath.append("*");
    searchPath.append(extension);

    WIN32_FIND_DATAA findData;
    HANDLE fileHandle = FindFirstFileA(searchPath.c_str(), &findData);

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        do
        {
            // Append the assembly to the list
            tpaList.append(directory);
            tpaList.append(FS_SEPARATOR);
            tpaList.append(findData.cFileName);
            tpaList.append(PATH_DELIMITER);

            // Note that the CLR does not guarantee which assembly will be loaded if an assembly
            // is in the TPA list multiple times (perhaps from different paths or perhaps with different NI/NI.dll
            // extensions. Therefore, a real host should probably add items to the list in priority order and only
            // add a file if it's not already present on the list.
            //
            // For this simple sample, though, and because we're only loading TPA assemblies from a single path,
            // and have no native images, we can ignore that complication.
        }
        while (FindNextFileA(fileHandle, &findData));
        FindClose(fileHandle);
    }
}
// </Snippet7>
#else
// POSIX directory search for .dll files
void BuildTpaList(const char* directory, const char* extension, std::string& tpaList)
{
    //  const char * const tpaExtensions[] = {
    //             ".ni.dll",      // Probe for .ni.dll first so that it's preferred if ni and il coexist in the same dir
    //             ".dll",
    //             ".ni.exe",
    //             ".exe",
    //             };

    // DIR* dir = opendir(directory);
    // if (dir == nullptr)
    // {
    //     return;
    // }

    // std::set<std::string> addedAssemblies;

    // // Walk the directory for each extension separately so that we first get files with .ni.dll extension,
    // // then files with .dll extension, etc.
    // for (int extIndex = 0; extIndex < sizeof(tpaExtensions) / sizeof(tpaExtensions[0]); extIndex++)
    // {
    //     const char* ext = tpaExtensions[extIndex];
    //     int extLength = strlen(ext);

    //     struct dirent* entry;

    //     // For all entries in the directory
    //     while ((entry = readdir(dir)) != nullptr)
    //     {
    //         // We are interested in files only
    //         switch (entry->d_type)
    //         {
    //         case DT_REG:
    //             break;

    //         // Handle symlinks and file systems that do not support d_type
    //         case DT_LNK:
    //         case DT_UNKNOWN:
    //             {
    //                 std::string fullFilename;

    //                 fullFilename.append(directory);
    //                 fullFilename.append("/");
    //                 fullFilename.append(entry->d_name);

    //                 struct stat sb;
    //                 if (stat(fullFilename.c_str(), &sb) == -1)
    //                 {
    //                     continue;
    //                 }

    //                 if (!S_ISREG(sb.st_mode))
    //                 {
    //                     continue;
    //                 }
    //             }
    //             break;

    //         default:
    //             continue;
    //         }

    //         std::string filename(entry->d_name);

    //         // Check if the extension matches the one we are looking for
    //         int extPos = filename.length() - extLength;
    //         if ((extPos <= 0) || (filename.compare(extPos, extLength, ext) != 0))
    //         {
    //             continue;
    //         }

    //         std::string filenameWithoutExt(filename.substr(0, extPos));

    //         // Make sure if we have an assembly with multiple extensions present,
    //         // we insert only one version of it.
    //         if (addedAssemblies.find(filenameWithoutExt) == addedAssemblies.end())
    //         {
    //             addedAssemblies.insert(filenameWithoutExt);

    //             tpaList.append(directory);
    //             tpaList.append("/");
    //             tpaList.append(filename);
    //             tpaList.append(":");
    //         }
    //     }
        
    //     // Rewind the directory stream to be able to iterate over it for the next extension
    //     rewinddir(dir);
    // }
    
    // closedir(dir);
    DIR* dir = opendir(directory);
    struct dirent* entry;
    int extLength = strlen(extension);

    while ((entry = readdir(dir)) != NULL)
    {
        // This simple sample doesn't check for symlinks
        std::string filename(entry->d_name);

        // Check if the file has the right extension
        int extPos = filename.length() - extLength;
        if (extPos <= 0 || filename.compare(extPos, extLength, extension) != 0)
        {
            continue;
        }

        // Append the assembly to the list
        tpaList.append(directory);
        tpaList.append(FS_SEPARATOR);
        tpaList.append(filename);
        tpaList.append(PATH_DELIMITER);

        // Note that the CLR does not guarantee which assembly will be loaded if an assembly
        // is in the TPA list multiple times (perhaps from different paths or perhaps with different NI/NI.dll
        // extensions. Therefore, a real host should probably add items to the list in priority order and only
        // add a file if it's not already present on the list.
        //
        // For this simple sample, though, and because we're only loading TPA assemblies from a single path,
        // and have no native images, we can ignore that complication.
    }

    closedir(dir);
}
#endif

// Callback function passed to managed code to facilitate calling back into native code with status
int ReportProgressCallback(int progress)
{
    // Just print the progress parameter to the console and return -progress
    printf("Received status from managed code: %d\n", progress);
    return -progress;
}

// #include <iostream>
// #include <limits.h>
// #include <stdlib.h>
// #include <dlfcn.h>
// #include <string.h>
// #include <set>
// #include <dirent.h>
// #include <sys/stat.h>

// #include "coreclrhost.h"

// using namespace std;

// typedef char *(*bootstrap_ptr)();

// void AddFilesFromDirectoryToTpaList(const char* directory, std::string& tpaList)
// {
//     const char * const tpaExtensions[] = {
//                 ".ni.dll",      // Probe for .ni.dll first so that it's preferred if ni and il coexist in the same dir
//                 ".dll",
//                 ".ni.exe",
//                 ".exe",
//                 };

//     DIR* dir = opendir(directory);
//     if (dir == nullptr)
//     {
//         return;
//     }

//     std::set<std::string> addedAssemblies;

//     // Walk the directory for each extension separately so that we first get files with .ni.dll extension,
//     // then files with .dll extension, etc.
//     for (int extIndex = 0; extIndex < sizeof(tpaExtensions) / sizeof(tpaExtensions[0]); extIndex++)
//     {
//         const char* ext = tpaExtensions[extIndex];
//         int extLength = strlen(ext);

//         struct dirent* entry;

//         // For all entries in the directory
//         while ((entry = readdir(dir)) != nullptr)
//         {
//             // We are interested in files only
//             switch (entry->d_type)
//             {
//             case DT_REG:
//                 break;

//             // Handle symlinks and file systems that do not support d_type
//             case DT_LNK:
//             case DT_UNKNOWN:
//                 {
//                     std::string fullFilename;

//                     fullFilename.append(directory);
//                     fullFilename.append("/");
//                     fullFilename.append(entry->d_name);

//                     struct stat sb;
//                     if (stat(fullFilename.c_str(), &sb) == -1)
//                     {
//                         continue;
//                     }

//                     if (!S_ISREG(sb.st_mode))
//                     {
//                         continue;
//                     }
//                 }
//                 break;

//             default:
//                 continue;
//             }

//             std::string filename(entry->d_name);

//             // Check if the extension matches the one we are looking for
//             int extPos = filename.length() - extLength;
//             if ((extPos <= 0) || (filename.compare(extPos, extLength, ext) != 0))
//             {
//                 continue;
//             }

//             std::string filenameWithoutExt(filename.substr(0, extPos));

//             // Make sure if we have an assembly with multiple extensions present,
//             // we insert only one version of it.
//             if (addedAssemblies.find(filenameWithoutExt) == addedAssemblies.end())
//             {
//                 addedAssemblies.insert(filenameWithoutExt);

//                 tpaList.append(directory);
//                 tpaList.append("/");
//                 tpaList.append(filename);
//                 tpaList.append(":");
//             }
//         }
        
//         // Rewind the directory stream to be able to iterate over it for the next extension
//         rewinddir(dir);
//     }
    
//     closedir(dir);
// }    

// int main(int argc, char *argv[])
// {
//     if (argc != 2)
//     {
//         cerr << "Usage: host <core_clr_path>" << endl;
//         return -1;
//     }

//     char appPath[PATH_MAX];
//     if (realpath(argv[0], appPath) == NULL)
//     {
//         cerr << "bad path " << argv[0] << endl;
//         return -1;
//     }

//     char *last_slash = strrchr(appPath, '/');
//     if (last_slash != NULL)
//         *last_slash = 0;

//     cout << "appPath:" << appPath << endl;

//     cout << "Loading CoreCLR..." << endl;

//     char pkg_path[PATH_MAX];
//     if (realpath(argv[1], pkg_path) == NULL)
//     {
//         cerr << "bad path " << argv[1] << endl;
//         return -1;
//     }

//      //
//     // Load CoreCLR
//     //
//     string coreclr_path(pkg_path);
//     coreclr_path.append("/libcoreclr.dylib");

//     cout << "coreclr_path:" << coreclr_path.c_str() << endl;

//     void *coreclr = dlopen(coreclr_path.c_str(), RTLD_NOW | RTLD_LOCAL);
//     if (coreclr == NULL)
//     {
//         cerr << "failed to open " << coreclr_path << endl;
//         cerr << "error: " << dlerror() << endl;
//         return -1;
//     }

//     //
//     // Initialize CoreCLR
//     //
//     std::cout << "Initializing CoreCLR..." << endl;

//     coreclr_initialize_ptr coreclr_init = reinterpret_cast<coreclr_initialize_ptr>(dlsym(coreclr, "coreclr_initialize"));
//     if (coreclr_init == NULL)
//     {
//         cerr << "couldn't find coreclr_initialize in " << coreclr_path << endl;
//         return -1;
//     }

//     string tpa_list;
//     AddFilesFromDirectoryToTpaList(pkg_path, tpa_list);

//     const char *property_keys[] = {
//         "appPathS",
//         "TRUSTED_PLATFORM_ASSEMBLIES"
//     };
//     const char *property_values[] = {
//         // appPathS
//         appPath,
//         // TRUSTED_PLATFORM_ASSEMBLIES
//         tpa_list.c_str()
//     };

//     void *coreclr_handle;
//     unsigned int domain_id;
//     int ret = coreclr_init(
//         appPath,                               // exePath
//         "host",                                 // appDomainFriendlyName
//         sizeof(property_values)/sizeof(char *), // propertyCount
//         property_keys,                          // propertyKeys
//         property_values,                        // propertyValues
//         &coreclr_handle,                        // hostHandle
//         &domain_id                              // domainId
//         );
//     if (ret < 0)
//     {
//         cerr << "failed to initialize coreclr. cerr = " << ret << endl;
//         return -1;
//     }

//     //
//     // Once CoreCLR is initialized, bind to the delegate
//     //
//     std::cout << "Creating delegate..." << endl;
//     coreclr_create_delegate_ptr coreclr_create_dele = reinterpret_cast<coreclr_create_delegate_ptr>(dlsym(coreclr, "coreclr_create_delegate"));
//     if (coreclr_create_dele == NULL)
//     {
//         cerr << "couldn't find coreclr_create_delegate in " << coreclr_path << endl;
//         return -1;
//     }

//     bootstrap_ptr dele;
//     ret = coreclr_create_dele(
//         coreclr_handle,
//         domain_id,
//         "manlib",
//         "ManLib",
//         "Bootstrap",
//         reinterpret_cast<void **>(&dele)
//         );
//     if (ret < 0)
//     {
//         cerr << "couldn't create delegate. err = " << ret << endl;
//         return -1;
//     }

//     //
//     // Call the delegate
//     //
//     cout << "Calling ManLib::Bootstrap() through delegate..." << endl;

//     char *msg = dele();
//     cout << "ManLib::Bootstrap() returned " << msg << endl;    
//     free(msg);      // returned string need to be free-ed
    
//     dlclose(coreclr);
// }
