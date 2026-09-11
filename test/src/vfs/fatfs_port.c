#include "stdio.h"
#include "ff.h"

int fatfs_init(void)
{
    static FATFS fatfs;
    FRESULT Res;
    TCHAR *Path = "0:/";
    BYTE work[FF_MAX_SS];
    MKFS_PARM opt = {0};
    /* Try to mount FAT file system */
    Res = f_mount(&fatfs, Path, 1);
    if (Res != FR_OK)
    {
        opt.fmt = FM_ANY;
        Res = f_mkfs(Path, &opt, work, sizeof(work));
        if (Res != FR_OK)
        {
            printf("f_mkfs failed\n");
            return -1;
        }
        Res = f_mount(&fatfs, Path, 1);
        if (Res != FR_OK)
        {
            printf("f_mount failed\n");
            return -1;
        }
    }
    printf("platform init fatfs success\n");
    return 0;
}
