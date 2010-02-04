/* dt.h - 'dir tree' engine
 * Walks directory subtree and counts size like du(1).
 * File system navigation functions as well as readdir are
 * provided externally through dt_walker structure
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "dt.h"
#include "logpp.h"

struct dt_dentry * dt_alloc()
{
    return (struct dt_dentry *) calloc(1, sizeof(struct dt_dentry));
}

void dt_free(struct dt_dentry *d)
{
    free(d->name);
    free(d);
}

static void dt_init_root(struct dt_dentry *root, unsigned int *id)
{
    root->parent = NULL;
    root->sibling = NULL;
    root->child = NULL;
    root->file_child = NULL;
    root->stamp = 0;
    root->id = (*id)++;
}

int dt_compare(const void *dd1, const void *dd2)
{
    struct dt_dentry *d1 = *(struct dt_dentry **) dd1;
    struct dt_dentry *d2 = *(struct dt_dentry **) dd2;
    return strcmp(d1->name, d2->name);
}

/* sorts list of dt_detnries by names and
 * assignes fids according to the order starting from s */
static struct dt_dentry * dt_list_sort(struct dt_dentry *d, size_t nmemb, unsigned int s)
{
    struct dt_dentry **da;
    struct dt_dentry *dp;
    size_t i;
    LOG_ASSERT((d != NULL) && (nmemb > 0), "Bad arguments\n");

    da = (struct dt_dentry **) malloc(nmemb * sizeof(struct dt_dentry *));
    for (da[0] = d, i = 1; i < nmemb; i++)
        da[i] = da[i-1]->sibling;
    qsort (da, nmemb, sizeof(struct dt_dentry *), dt_compare);
    for (i = 1, dp = da[0], dp->fid = s++; i < nmemb; i++) {
        dp->sibling = da[i];
        dp = da[i];
        dp->fid = s++;
    }
    dp->sibling = NULL;
    dp = da[0];
    free(da);
    return dp;
}

/* print child list, add sizes to parent and free childs
 * d - parent
 * c - pointer to child list pointer in d structure
 */
static void dt_list_print_sum_free(struct dt_dentry *d, struct dt_dentry **c, void (*printfile) (struct dt_dentry *))
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT((d != NULL) && (c != NULL), "Bad arguments\n");
    *c = NULL;
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        printfile(dp);
        d->size += dp->size;
        dt_free(dp);
    }
}

static void dt_list_sum(struct dt_dentry *d, struct dt_dentry **c)
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT((d != NULL) && (c != NULL), "Bad arguments\n");
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        d->size += dp->size;
    }
}

static void dt_list_print_free(struct dt_dentry **c, void (*printfile) (struct dt_dentry *))
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT(c != NULL, "Bad arguments\n");
    *c = NULL;
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        printfile(dp);
        dt_free(dp);
    }
}

static void dt_list_print(struct dt_dentry **c, void (*printfile) (struct dt_dentry *))
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT(c != NULL, "Bad arguments\n");
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        printfile(dp);
    }
}

static void dt_list_free(struct dt_dentry **c)
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT(c != NULL, "Bad arguments\n");
    *c = NULL;
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        dt_free(dp);
    }
}

/* link next read child into dir tree according to selectd out policy
 * c - pointer to list in current directory structure
 * dp - last linked child (NULL if none)
 * dn - newly read child
 *
 * return: dn, if dn linked, dc otherwise
 */
static struct dt_dentry * dt_linkchild(struct dt_dentry **c, struct dt_dentry *dp, struct dt_dentry *dn)
{
    LOG_ASSERT((c != NULL) && (dn != NULL), "Bad arguments\n");
    if (dp == NULL)
        *c = dn;
    else
        dp->sibling = dn;
    return dn;
}

static void dt_readdir(struct dt_walker *wk, struct dt_dentry *d, void *curdir, unsigned int *id)
{
    struct dt_dentry *ddc = NULL, *dfc = NULL, *dn;
    unsigned int dirs = 0;
    unsigned int files = 0;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");
    
    while ((dn = wk->readdir(curdir)) != NULL) {
        dn->stamp = 0;
        dn->parent = d;
        if (dn->type == DT_DIR) {
            dirs++;
            dn->id = (*id)++;
            ddc = dt_linkchild(&(d->child), ddc, dn);
        } else {
            files++;
            dfc = dt_linkchild(&(d->file_child), dfc, dn);
        }
    }
    if (ddc != NULL)
        ddc->sibling = NULL;
    if (dfc != NULL)
        dfc->sibling = NULL;

    if (dirs > 0)
        d->child = dt_list_sort(d->child, dirs, 0);
    if (files > 0)
        d->file_child = dt_list_sort(d->file_child, files, dirs);
    d->items = files + dirs;
}

static struct dt_dentry * dt_go_sibling_or_parent(struct dt_walker *wk, struct dt_dentry *d, void *curdir)
{
    struct dt_dentry *dn = d;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");
    while (((dn = dn->sibling) != NULL)
           && (wk->go(DT_GO_SIBLING, dn->name, curdir) < 0))
        ;
    if (dn == NULL) {
        dn = d->parent;
        if ((dn != NULL) && (wk->go(DT_GO_PARENT, NULL, curdir) < 0))
            exit(EXIT_FAILURE);
    }
    return dn;
}

static struct dt_dentry * dt_go_child(struct dt_walker *wk, struct dt_dentry *d, void *curdir)
{
    struct dt_dentry *dc;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");
    if ((dc = d->child) == NULL)
        return NULL;
    while ((wk->go(DT_GO_CHILD, dc->name, curdir) < 0)
           && ((dc = dc->sibling) != NULL))
        ;
    return dc;
}

static struct dt_dentry * dt_next_sibling_or_parent(struct dt_dentry *d)
{
    LOG_ASSERT(d != NULL, "Bad arguments\n");
    if (d->sibling != NULL)
        return d->sibling;
    else
        return d->parent;
}

static void dt_printpath(struct dt_dentry *d)
{
    LOG_ASSERT(d != NULL, "Bad arguments\n");
    if ((d->parent != NULL) && (d->parent->name[0] != 0) ) {
        dt_printpath(d->parent);
        printf("/");
    }
    printf("%s",d->name);
}

static void dt_printfile_full(struct dt_dentry *d)
{
    dt_printpath(d);
    printf("%s %llu\n", (d->type == DT_DIR) ? "/" : "", d->size);
}

void dt_full(struct dt_walker *wk, struct dt_dentry *root, void *curdir)
{
    struct dt_dentry *d, *dc;
    unsigned int id = 1;

    if ((root == NULL) || (wk == NULL)) {
        LOG_ERR("Bad arguments\n");
        return;
    }

    dt_init_root(root, &id);

    /* invariants:
     * d->type == DT_DIR
     * if v->stamp = 1 then descendants of v are processed
     */
    for (d = root; d != NULL;) {
        if (d->stamp == 0) {
            dt_readdir(wk, d, curdir, &id);
            if ((dc = dt_go_child(wk, d, curdir)) != NULL) {
                d->stamp = 1;
                d = dc;
                continue;
            }
        } else {
            d->stamp = 0;
        }
        dt_list_sum(d, &(d->file_child));
        dt_list_sum(d, &(d->child));
        d = dt_go_sibling_or_parent(wk, d, curdir);
    }

    dt_printfile_full(root);
    for (d = root; d != NULL;) {
        if (d->stamp == 0) {
            dt_list_print(&(d->child), dt_printfile_full);
            dt_list_print_free(&(d->file_child), dt_printfile_full);
            if ((dc = d->child) != NULL) {
                d->stamp = 1;
                d = dc;
                continue;
            }
        } else { 
            d->stamp = 0;
        }
        dt_list_free(&(d->child));
        d = dt_next_sibling_or_parent(d);
    }
}

static void dt_printdir_reverse(struct dt_dentry *d)
{
    printf("0 %u ", d->id);
    dt_printpath(d);
    printf("\n");
}

static void dt_printfile_reverse(struct dt_dentry *d)
{
    printf("1 %u %u %llu %u %u ",
            (d->parent != NULL) ? d->parent->id : 0,
            d->fid, d->size,
            (d->type == DT_DIR) ? d->id : 0,
            (d->type == DT_DIR) ? d->items : 0
            );
    printf("%s", d->name);
    printf("\n");
}

void dt_reverse(struct dt_walker *wk, struct dt_dentry *root, void *curdir)
{
    struct dt_dentry *d, *dc;
    unsigned int id = 1;

    if ((root == NULL) || (wk == NULL)) {
        LOG_ERR("Bad arguments\n");
        return;
    }

    dt_init_root(root, &id);
    dt_printdir_reverse(root);

    for (d = root; d != NULL;) {
        if (d->stamp == 0) {
            dt_readdir(wk, d, curdir, &id);
            dt_list_print(&(d->child), dt_printdir_reverse);
            dt_list_print_sum_free(d, &(d->file_child), dt_printfile_reverse);
            if ((dc = dt_go_child(wk, d, curdir)) != NULL) {
                d->stamp = 1;
                d = dc;
                continue;
            }
        } else {
            d->stamp = 0;
        }
        dt_list_print_sum_free(d, &(d->child), dt_printfile_reverse);
        d = dt_go_sibling_or_parent(wk, d, curdir);
    }

    dt_printfile_reverse(root);
}

