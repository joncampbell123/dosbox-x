/***********************************************************************/
/* MANEXT - Extract manual pages from C source code.                   */
/***********************************************************************/
/*
 * MANEXT - A program to extract manual pages from C source code.
 * Copyright (C) 1991-1996 Mark Hessling
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * If you make modifications to this software that you feel increases
 * it usefulness for the rest of the community, please email the
 * changes, enhancements, bug fixes as well as any and all ideas to me.
 * This software is going to be maintained and enhanced as deemed
 * necessary by the community.
 *
 * Mark Hessling <mark@rexx.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 255

void display_info()
{
    fprintf(stderr, "\nMANEXT 1.03 Copyright (C) 1991-1996 Mark Hessling\n"
                    "All rights reserved.\n"
                    "MANEXT is distributed under the terms of the GNU\n"
                    "General Public License and comes with NO WARRANTY.\n"
                    "See the file COPYING for details.\n"
                    "\nUsage: manext sourcefile [...]\n\n");
}

int main(int argc, char **argv)
{
    char s[MAX_LINE + 1];       /* input line */
    int i;
    FILE *fp;

#ifdef __EMX__
    _wildcard(&argc, &argv);
#endif
    if (strcmp(argv[1], "-h") == 0)
    {
        display_info();
        exit(1);
    }

    for (i = 1; i < argc; i++)
    {
        if ((fp = fopen(argv[i], "r")) == NULL)
        {
            fprintf(stderr, "\nCould not open %s\n", argv[i]);
            continue;
        }

        while (!feof(fp))
        {
            if (fgets(s, (int)sizeof(s), fp) == NULL)
            {
                if (ferror(fp) != 0)
                {
                    fprintf(stderr, "*** Error reading %s.  Exiting.\n",
                            argv[i]);
                    exit(1);
                }

                break;
            }

            /* check for manual entry marker at beginning of line */

            if (strncmp(s, "/*man-start*", 12) != 0)
                continue;

            /* inner loop */

            for (;;)
            {
                /* read next line of manual entry */

                if (fgets(s, (int)sizeof(s), fp) == NULL)
                {
                    if (ferror(fp) != 0)
                    {
                        fprintf(stderr, "*** Error reading %s.  Exiting.\n",
                                argv[i]);
                        exit(1);
                    }

                    break;
                }

                /* check for end of entry marker */

                if (strncmp(s, "**man-end", 9) == 0)
                    break;

                printf("%s", s);
            }

            printf("\n\n-----------------------------------"
                   "---------------------------------------\n\n");
        }

        fclose(fp);
    }

    return 0;
}
