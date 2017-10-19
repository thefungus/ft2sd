#ifndef _SD_DUMPER_H_
#define _SD_DUMPER_H_

/* Main */
#ifdef __cplusplus
extern "C" {
#endif

int DumpDir(char *pPath, const char * target_path);
int StartDump(char *pPath, const char * target_path, const int selectedItem);

#ifdef __cplusplus
}
#endif

#endif
