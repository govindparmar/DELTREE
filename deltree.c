// Compile with:
// cl deltree.c /O2t /DUNICODE /D_UNICODE /DNDEBUG /link user32.lib [USING_PATHCCH ? pathcch.lib : shlwapi.lib] /DEBUG:NONE /MANIFEST:EMBED /OUT:Deltree.exe
// If USING_PATHCCH is defined (> Windows 7) `PathCchCombine` from pathcch library is used, else `PathCombineW` from shlwapi is used
// Add /DCASCADE_UNEXPECTED_ERROR before /link to bubble the error up and stop when one item fails to delete for any reason other than "file in use" or "access denied"
// Govind Parmar
#define CASCADE_UNEXPECTED_ERROR
#define USING_PATHCCH
#include <Windows.h>
#ifdef USING_PATHCCH
#include <pathcch.h>
#else
#include <Shlwapi.h>
#endif
#include <stdio.h>
#include <strsafe.h>
#include <sal.h>

#define WMain wmain

DWORD WINAPI RecursivelyDeleteDirectory(_In_reads_or_z_(MAX_PATH) WCHAR *wszDirectory)
{
	HANDLE hFind = INVALID_HANDLE_VALUE;
	WCHAR wszPattern[MAX_PATH];
	DWORD dwError;
	WIN32_FIND_DATAW wfd;

#ifdef USING_PATHCCH
	PathCchCombine(wszPattern, MAX_PATH, wszDirectory, L"*");
#else
	PathCombineW(wszPattern, wszDirectory, L"*");
#endif
	hFind = FindFirstFileW(wszPattern, &wfd);
	if (INVALID_HANDLE_VALUE == hFind)
	{
		dwError = GetLastError();
		goto cleanup;
	}
	do
	{
#ifdef USING_PATHCCH
		PathCchCombine(wszPattern, MAX_PATH, wszDirectory, wfd.cFileName);
#else
		PathCombineW(wszPattern, wszDirectory, wfd.cFileName);
#endif
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (L'.' == wfd.cFileName[0] && (L'.' == wfd.cFileName[1] || L'\0' == wfd.cFileName[1]))
			{
				continue;
			}

			dwError = RecursivelyDeleteDirectory(wszPattern);
			if (dwError != ERROR_SUCCESS)
			{
#ifdef CASCADE_UNEXPECTED_ERROR
				if (dwError != ERROR_ACCESS_DENIED && dwError != ERROR_SHARING_VIOLATION)
				{
					goto cleanup;
				}
#endif
				continue;
			}
			if (!RemoveDirectoryW(wszPattern))
			{
				dwError = GetLastError();
				fwprintf(stderr, L"Could not remove directory %s: %I32u\n", wszPattern, dwError);
#ifdef CASCADE_UNEXPECTED_ERROR
				if (dwError != ERROR_ACCESS_DENIED && dwError != ERROR_SHARING_VIOLATION)
				{
					goto cleanup;
				}
#endif
				continue;
			}
		}
		else
		{
			if (!DeleteFileW(wszPattern))
			{
				dwError = GetLastError();
				fwprintf(stderr, L"Could not remove file %s: %I32u\n", wszPattern, dwError);
#ifdef CASCADE_UNEXPECTED_ERROR
				if(dwError != ERROR_ACCESS_DENIED && dwError != ERROR_SHARING_VIOLATION)
				{
					goto cleanup;
				}
#endif
				continue;
			}
		}
	}
	while (FindNextFileW(hFind, &wfd));
	dwError = ERROR_SUCCESS;
cleanup:
	if (hFind != INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
	}
	return dwError;
}

INT WINAPIV WMain(_In_ INT nArgc, _In_reads_(nArgc) WCHAR *pArgv[])
{
	DWORD dwError;
	WCHAR wszRootDir[MAX_PATH];
	if (nArgc < 2)
	{
		GetCurrentDirectoryW(MAX_PATH, wszRootDir);
	}
	else
	{
		StringCchCopyW(wszRootDir, MAX_PATH, pArgv[1]);
	}
	if ((dwError = RecursivelyDeleteDirectory(wszRootDir)) != ERROR_SUCCESS)
	{
		fwprintf(stderr, L"Failed to recursively remove directory contents: %I32u\n", dwError);
	}
	if (!RemoveDirectoryW(wszRootDir))
	{
		dwError = GetLastError();
		fwprintf(stderr, L"Could remove directory tree within start directory %s, but not %s itself: %I32u\n", wszRootDir, wszRootDir, dwError);
	}
	return (INT) dwError;
}
