/*
 * Copyright (c) <2015-2016>, <Danilo Chang>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <tcl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef USE_SYSTEM_LMDB
#include <lmdb.h>
#else
#include "lmdb.h"
#endif

/*
 * Windows needs to know which symbols to export.  Unix does not.
 * BUILD_lmdb should be undefined for Unix.
 */
#ifdef BUILD_lmdb
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#endif /* BUILD_lmdb */

typedef struct ThreadSpecificData {
  int initialized;                /* initialization flag */
  Tcl_HashTable *lmdb_hashtblPtr; /* per thread hash table. */
  int env_count;
  int txn_count;
  int dbi_count;
  int cur_count;
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

TCL_DECLARE_MUTEX(myMutex);


void LMDB_Thread_Exit(ClientData clientdata)
{
  ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

  if(tsdPtr->lmdb_hashtblPtr) {
    Tcl_DeleteHashTable(tsdPtr->lmdb_hashtblPtr);
    ckfree(tsdPtr->lmdb_hashtblPtr);
  }
}


static int LMDB_CUR(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
  int choice;
  int result;
  MDB_cursor *cursor;
  Tcl_HashEntry *hashEntryPtr;
  char *curHandle;

  ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

  if (tsdPtr->initialized == 0) {
    tsdPtr->initialized = 1;
    tsdPtr->lmdb_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tsdPtr->lmdb_hashtblPtr, TCL_STRING_KEYS);
  }

  static const char *CUR_strs[] = {
    "get",
    "put",
    "del",
    "count",
    "renew",
    "close",
    0
  };

  enum CUR_enum {
    CUR_GET,
    CUR_PUT,
    CUR_DEL,
    CUR_COUNT,
    CUR_RENEW,
    CUR_CLOSE,
  };

  if( objc < 2 ){
    Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
    return TCL_ERROR;
  }

  if( Tcl_GetIndexFromObj(interp, objv[1], CUR_strs, "option", 0, &choice) ){
    return TCL_ERROR;
  }

  /*
   * Get the MDB_cursor * point
   */
  curHandle = Tcl_GetStringFromObj(objv[0], 0);
  hashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, curHandle );
  if( !hashEntryPtr ) {
    if( interp ) {
        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
        Tcl_AppendStringsToObj( resultObj, "invalid cursor handle ", curHandle, (char *)NULL );
    }

    return TCL_ERROR;
  }

  cursor = Tcl_GetHashValue( hashEntryPtr );

  switch( (enum CUR_enum)choice ){

    case CUR_GET: {
      char *zArg;
      unsigned char *key;
      unsigned char *data;
      int len;
      MDB_val mkey;
      MDB_val mdata;
      MDB_cursor_op op;
      int need_key = 0;
      int need_key_data = 0;
      Tcl_Obj *pResultStr = NULL;

      if( objc < 3){
        Tcl_WrongNumArgs(interp, 2, objv,
        "?-set? ?-set_range? ?-current? ?-first? ?-firstdup? ?-last? ?-lastdup? \
         ?-next? ?-nextdup? ?-nextnodup? ?-prev? ?-prevdup? ?-prevnodup? \
         ?-get_multiple? ?-next_multiple? ?-get_both? ?-get_both_range? ?key? ?data?");
        return TCL_ERROR;
      }

      zArg = Tcl_GetString(objv[2]);
      if( strcmp(zArg, "-set")==0 ){
          op = MDB_SET;
          need_key = 1;
      } else if( strcmp(zArg, "-set_range")==0 ){
          op = MDB_SET_RANGE;
          need_key = 1;
      } else if( strcmp(zArg, "-current")==0 ){
          op = MDB_GET_CURRENT;
      } else if( strcmp(zArg, "-first")==0 ){
          op = MDB_FIRST;
      } else if( strcmp(zArg, "-firstdup")==0 ){
          op = MDB_FIRST_DUP;
      } else if( strcmp(zArg, "-last")==0 ){
          op = MDB_LAST;
      } else if( strcmp(zArg, "-lastdup")==0 ){
          op = MDB_LAST_DUP;
      } else if( strcmp(zArg, "-next")==0 ){
          op = MDB_NEXT;
      } else if( strcmp(zArg, "-nextdup")==0 ){
          op = MDB_NEXT_DUP;
      } else if( strcmp(zArg, "-nextnodup")==0 ){
          op = MDB_NEXT_NODUP;
      } else if( strcmp(zArg, "-prev")==0 ){
          op = MDB_PREV;
      } else if( strcmp(zArg, "-prevdup")==0 ){
          op = MDB_PREV_DUP ;
      } else if( strcmp(zArg, "-prevnodup")==0 ){
          op = MDB_PREV_NODUP;
      } else if( strcmp(zArg, "-get_multiple")==0 ){
          /*
           * MDB_GET_MULTIPLE only for MDB_DUPFIXED
           */

          op = MDB_GET_MULTIPLE;
          need_key_data = 1;
      } else if( strcmp(zArg, "-next_multiple")==0 ){
          op = MDB_NEXT_MULTIPLE;
          need_key_data = 1;
      } else if( strcmp(zArg, "-get_both")==0 ){
          op = MDB_GET_BOTH;
          need_key_data = 1;
      } else if( strcmp(zArg, "-get_both_range")==0 ){
          op = MDB_GET_BOTH_RANGE;
          need_key_data = 1;
      } else{
          Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
          return TCL_ERROR;
      }

      if(need_key) {
         if( objc != 4 && objc != 5){
           if( interp ) {
              Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
              Tcl_AppendStringsToObj( resultObj, "Wrong option: need key", (char *)NULL );
           }

           return TCL_ERROR; // This option need a key
         }

         key = Tcl_GetByteArrayFromObj(objv[3], &len);
         if( !key || len < 1 ){
           return TCL_ERROR;
         }

         mkey.mv_size = len;
         mkey.mv_data = key;
      }

      if(need_key_data) {
         if( objc != 5 ){
           if( interp ) {
              Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
              Tcl_AppendStringsToObj( resultObj, "Wrong option: need key data", (char *)NULL );
           }

           return TCL_ERROR; // This option need a key and data
         }

         key = Tcl_GetByteArrayFromObj(objv[3], &len);
         if( !key || len < 1 ){
           return TCL_ERROR;
         }

         mkey.mv_size = len;
         mkey.mv_data = key;

         data = Tcl_GetByteArrayFromObj(objv[4], &len);
         if( !data || len < 1 ){
           return TCL_ERROR;
         }

         mdata.mv_size = len;
         mdata.mv_data = data;
      }

      result = mdb_cursor_get(cursor, &mkey, &mdata, op);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      pResultStr = Tcl_NewListObj(0, NULL);
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewByteArrayObj(mkey.mv_data, mkey.mv_size));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewByteArrayObj(mdata.mv_data, mdata.mv_size));

      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

    case CUR_PUT: {
      char *zArg;
      unsigned char *key;
      unsigned char *data;
      int key_len;
      int data_len;
      MDB_val mkey;
      MDB_val mdata;
      int flags = 0;
      int i = 0;

      if( objc < 4 || (objc&1)!=0) {
        Tcl_WrongNumArgs(interp, 2, objv, "key data ?-current boolean? ?-nodupdata boolean? ?-nooverwrite boolean? ?-append boolean? ?-appenddup boolean? ");
        return TCL_ERROR;
      }

      key = Tcl_GetByteArrayFromObj(objv[2], &key_len);
      if( !key || key_len < 1 ){
         return TCL_ERROR;
      }

      data = Tcl_GetByteArrayFromObj(objv[3], &data_len);
      if( !data || data_len < 1 ){
         return TCL_ERROR;
      }

      for(i=4; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);

        if( strcmp(zArg, "-current")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_CURRENT;
            }else{
              flags &= ~MDB_CURRENT;
            }
        } else if( strcmp(zArg, "-nodupdata")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_NODUPDATA;
            }else{
              flags &= ~MDB_NODUPDATA;
            }
        } else if( strcmp(zArg, "-nooverwrite")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_NOOVERWRITE;
            }else{
              flags &= ~MDB_NOOVERWRITE;
            }
        } else if( strcmp(zArg, "-append")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_APPEND;
            }else{
              flags &= ~MDB_APPEND;
            }
        } else if( strcmp(zArg, "-appenddup")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_APPENDDUP;
            }else{
              flags &= ~MDB_APPENDDUP;
            }
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      mkey.mv_size = key_len;
      mkey.mv_data = key;

      mdata.mv_size = data_len;
      mdata.mv_data = data;

      result = mdb_cursor_put(cursor, &mkey, &mdata, flags);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case CUR_DEL: {
      char *zArg;
      int flags = 0;
      int i = 0;

      if( objc != 2 && objc != 4){
        Tcl_WrongNumArgs(interp, 2, objv, "?-nodupdata boolean? ");
        return TCL_ERROR;
      }

      for(i=2; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);
        if( strcmp(zArg, "-nodupdata")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_NODUPDATA;
            }else{
              flags &= ~MDB_NODUPDATA;
            }
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      result = mdb_cursor_del(cursor, flags);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case CUR_COUNT: {
      size_t count;
      Tcl_Obj *pResultStr = NULL;

      if( objc != 2){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      result = mdb_cursor_count(cursor, &count);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      pResultStr = Tcl_NewIntObj( count );
      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

    case CUR_RENEW: {
      char *zArg;
      MDB_txn *txn;
      Tcl_HashEntry *txnHashEntryPtr;
      char *txnHandle = NULL;

      if( objc != 4){
        Tcl_WrongNumArgs(interp, 2, objv, "-txn txnid ");
        return TCL_ERROR;
      }

      zArg = Tcl_GetString(objv[2]);
      if( strcmp(zArg, "-txn")==0 ){
         txnHandle = Tcl_GetStringFromObj(objv[3], 0);
      } else{
         Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
         return TCL_ERROR;
      }

      if(!txnHandle) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
          Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", (char *)NULL );
        }

        return TCL_ERROR;
      }

      txnHashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, txnHandle );
      if( !txnHashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", txnHandle, (char *)NULL );
        }

        return TCL_ERROR;
      }

      txn = Tcl_GetHashValue( txnHashEntryPtr );
      result = mdb_cursor_renew(txn, cursor);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case CUR_CLOSE: {
      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      mdb_cursor_close (cursor);

      Tcl_MutexLock(&myMutex);
      if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
      Tcl_MutexUnlock(&myMutex);
      Tcl_DeleteCommand(interp, curHandle);

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

  }

  return TCL_OK;
}


static int LMDB_DBI(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
  int choice;
  int result;
  MDB_dbi dbi;
  Tcl_HashEntry *hashEntryPtr;
  char *dbiHandle;

  ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

  if (tsdPtr->initialized == 0) {
    tsdPtr->initialized = 1;
    tsdPtr->lmdb_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tsdPtr->lmdb_hashtblPtr, TCL_STRING_KEYS);
  }

  static const char *DBI_strs[] = {
    "put",
    "get",
    "del",
    "drop",
    "close",
    "stat",
    "cursor",
    0
  };

  enum DBI_enum {
    DBI_PUT,
    DBI_GET,
    DBI_DEL,
    DBI_DROP,
    DBI_CLOSE,
    DBI_STAT,
    DBI_CURSOR,
  };

  if( objc < 2 ){
    Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
    return TCL_ERROR;
  }

  if( Tcl_GetIndexFromObj(interp, objv[1], DBI_strs, "option", 0, &choice) ){
    return TCL_ERROR;
  }

  /*
   * Get the MDB_dbi value
   */
  dbiHandle = Tcl_GetStringFromObj(objv[0], 0);
  hashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, dbiHandle );
  if( !hashEntryPtr ) {
    if( interp ) {
        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );

        Tcl_AppendStringsToObj( resultObj, "invalid dbi handle ", dbiHandle, (char *)NULL );
    }

    return TCL_ERROR;
  }

  dbi = Tcl_GetHashValue( hashEntryPtr );

  switch( (enum DBI_enum)choice ){

    case DBI_PUT: {
      unsigned char *key;
      unsigned char *data;
      int key_len;
      int data_len;
      MDB_val mkey;
      MDB_val mdata;
      const char *zArg;
      MDB_txn *txn;
      Tcl_HashEntry *txnHashEntryPtr;
      char *txnHandle = NULL;
      int flags = 0;
      int i = 0;

      if( objc < 4 || (objc&1)!=0 ){
        Tcl_WrongNumArgs(interp, 2, objv, "key data -txn txnid ?-nodupdata boolean? ?-nooverwrite boolean? ?-append boolean? ?-appenddup boolean? ");
        return TCL_ERROR;
      }

      key = Tcl_GetByteArrayFromObj(objv[2], &key_len);
      if( !key || key_len < 1 ){
        return TCL_ERROR;
      }

      data = Tcl_GetByteArrayFromObj(objv[3], &data_len);
      if( !data || data_len < 1 ){
        return TCL_ERROR;
      }

      for(i=4; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);
        if( strcmp(zArg, "-txn")==0 ){
            txnHandle = Tcl_GetStringFromObj(objv[i+1], 0);
        } else if( strcmp(zArg, "-nodupdata")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_NODUPDATA;
            }else{
              flags &= ~MDB_NODUPDATA;
            }
        } else if( strcmp(zArg, "-nooverwrite")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_NOOVERWRITE;
            }else{
              flags &= ~MDB_NOOVERWRITE;
            }
        } else if( strcmp(zArg, "-append")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_APPEND;
            }else{
              flags &= ~MDB_APPEND;
            }
        } else if( strcmp(zArg, "-appenddup")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_APPENDDUP;
            }else{
              flags &= ~MDB_APPENDDUP;
            }
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      if(!txnHandle) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
          Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", (char *)NULL );
        }

        return TCL_ERROR;
      }

      txnHashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, txnHandle );
      if( !txnHashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", txnHandle, (char *)NULL );
        }

        return TCL_ERROR;
      }

      txn = Tcl_GetHashValue( txnHashEntryPtr );

      mkey.mv_size = key_len;
      mkey.mv_data = key;
      mdata.mv_size = data_len;
      mdata.mv_data = data;

      result = mdb_put (txn, dbi, &mkey, &mdata, flags);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBI_GET: {
      unsigned char *key;
      int len;
      MDB_val mkey;
      MDB_val mdata;
      const char *zArg;
      MDB_txn *txn;
      Tcl_HashEntry *txnHashEntryPtr;
      char *txnHandle = NULL;
      int i = 0;
      Tcl_Obj *pResultStr;

      if( objc != 5 ){
        Tcl_WrongNumArgs(interp, 2, objv, "key -txn txnid ");
        return TCL_ERROR;
      }

      key = Tcl_GetByteArrayFromObj(objv[2], &len);
      if( !key || len < 1 ){
        return TCL_ERROR;
      }

      for(i=3; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);

        if( strcmp(zArg, "-txn")==0 ){
            txnHandle = Tcl_GetStringFromObj(objv[i+1], 0);
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      if(!txnHandle) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
          Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", (char *)NULL );
        }

        return TCL_ERROR;
      }

      txnHashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, txnHandle );
      if( !txnHashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", txnHandle, (char *)NULL );
        }

        return TCL_ERROR;
      }

      txn = Tcl_GetHashValue( txnHashEntryPtr );

      mkey.mv_size = len;
      mkey.mv_data = key;

      result = mdb_get (txn, dbi, &mkey, &mdata);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      pResultStr = Tcl_NewByteArrayObj( mdata.mv_data, mdata.mv_size );
      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

    case DBI_DEL: {
      unsigned char *key;
      unsigned char *data;
      int key_len;
      int data_len;
      MDB_val mkey;
      MDB_val mdata;
      int isEmptyData = 0;
      const char *zArg;
      MDB_txn *txn;
      Tcl_HashEntry *txnHashEntryPtr;
      char *txnHandle = NULL;
      int i = 0;

      if( objc < 6 ){
        Tcl_WrongNumArgs(interp, 2, objv, "key data -txn txnid");
        return TCL_ERROR;
      }

      key = Tcl_GetByteArrayFromObj(objv[2], &key_len);
      if( !key || key_len < 1 ){
        return TCL_ERROR;
      }

      /*
       * Improve this command behavior.
       * If user indicates data is an empty string "", don't return error.
       */
      data = Tcl_GetByteArrayFromObj(objv[3], &data_len);
      if( !data || data_len < 1 ){
        //return TCL_ERROR;
        isEmptyData = 1;
      }

      for(i=4; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);
        if( strcmp(zArg, "-txn")==0 ){
            txnHandle = Tcl_GetStringFromObj(objv[i+1], 0);
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      if(!txnHandle) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
          Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", (char *)NULL );
        }

        return TCL_ERROR;
      }

      txnHashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, txnHandle );
      if( !txnHashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", txnHandle, (char *)NULL );
        }

        return TCL_ERROR;
      }

      txn = Tcl_GetHashValue( txnHashEntryPtr );

      mkey.mv_size = key_len;
      mkey.mv_data = key;

      /*
       * Improve this command behavior.
       * Data is not a empty data, get it.
       */
      if(isEmptyData == 0) {
        mdata.mv_size = data_len;
        mdata.mv_data = data;
      }

      /*
       * Improve this command behavior.
       * If the database supports sorted duplicates and the data parameter is NULL,
       * all of the duplicate data items for the key will be deleted.
       * Otherwise, if the data parameter is non-NULL only the matching data item
       * will be deleted.
       */
      if(isEmptyData == 0) {
        result = mdb_del (txn, dbi, &mkey, &mdata);
      } else {
        result = mdb_del (txn, dbi, &mkey, NULL);
      }
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBI_DROP: {
      int del_flag;
      const char *zArg;
      MDB_txn *txn;
      Tcl_HashEntry *txnHashEntryPtr;
      char *txnHandle = NULL;
      int i = 0;

      if( objc < 5 ){
        Tcl_WrongNumArgs(interp, 2, objv, "del_flag -txn txnid");
        return TCL_ERROR;
      }

      Tcl_GetIntFromObj(interp, objv[2], &del_flag);

      for(i=3; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);

        if( strcmp(zArg, "-txn")==0 ){
            txnHandle = Tcl_GetStringFromObj(objv[i+1], 0);
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      if(!txnHandle) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
          Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", (char *)NULL );
        }

        return TCL_ERROR;
      }

      txnHashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, txnHandle );
      if( !txnHashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", txnHandle, (char *)NULL );
        }

        return TCL_ERROR;
      }

      txn = Tcl_GetHashValue( txnHashEntryPtr );
      result = mdb_drop (txn, dbi, del_flag);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBI_CLOSE: {
      const char *zArg;
      MDB_env *env;
      Tcl_HashEntry *envHashEntryPtr;
      char *envHandle;
      int i = 0;

      if( objc != 4 ){
        Tcl_WrongNumArgs(interp, 2, objv, "-env env_handle");
        return TCL_ERROR;
      }

      for(i=2; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);

        if( strcmp(zArg, "-env")==0 ){
            envHandle = Tcl_GetStringFromObj(objv[i+1], 0);
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      envHashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, envHandle );
      if( !envHashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid env handle ", envHandle, (char *)NULL );
        }

        return TCL_ERROR;
      }

      env = Tcl_GetHashValue( envHashEntryPtr );
      mdb_dbi_close(env, dbi);
      Tcl_MutexLock(&myMutex);
      if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
      Tcl_MutexUnlock(&myMutex);
      Tcl_DeleteCommand(interp, dbiHandle);

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBI_STAT: {
      const char *zArg;
      MDB_txn *txn;
      MDB_stat stat;
      Tcl_HashEntry *txnHashEntryPtr;
      char *txnHandle = NULL;
      int i = 0;
      Tcl_Obj *pResultStr = NULL;

      if( objc != 4 ){
        Tcl_WrongNumArgs(interp, 2, objv, "-txn txnid ");
        return TCL_ERROR;
      }

      for(i=2; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);
        if( strcmp(zArg, "-txn")==0 ){
            txnHandle = Tcl_GetStringFromObj(objv[i+1], 0);
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      if(!txnHandle) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
          Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", (char *)NULL );
        }

        return TCL_ERROR;
      }

      txnHashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, txnHandle );
      if( !txnHashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", txnHandle, (char *)NULL );
        }

        return TCL_ERROR;
      }

      txn = Tcl_GetHashValue( txnHashEntryPtr );
      result = mdb_stat(txn, dbi, &stat);
      if(result != 0) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );

          Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      pResultStr = Tcl_NewListObj(0, NULL);
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_psize));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_depth));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_branch_pages));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_leaf_pages));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_overflow_pages));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_entries));

      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

    case DBI_CURSOR: {
      const char *zArg;
      MDB_txn *txn;
      Tcl_HashEntry *txnHashEntryPtr;
      char *txnHandle = NULL;
      Tcl_HashEntry *newHashEntryPtr;
      char handleName[16 + TCL_INTEGER_SPACE];
      Tcl_Obj *pResultStr = NULL;
      int newvalue;
      int i = 0;

      MDB_cursor *cursor;

      if( objc != 4 ){
        Tcl_WrongNumArgs(interp, 2, objv, "-txn txnid");
        return TCL_ERROR;
      }

      for(i=2; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);
        if( strcmp(zArg, "-txn")==0 ){
            txnHandle = Tcl_GetStringFromObj(objv[i+1], 0);
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      if(!txnHandle) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
          Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", (char *)NULL );
        }

        return TCL_ERROR;
      }

      txnHashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, txnHandle );
      if( !txnHashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", txnHandle, (char *)NULL );
        }

        return TCL_ERROR;
      }

      txn = Tcl_GetHashValue( txnHashEntryPtr );
      result = mdb_cursor_open(txn, dbi, &cursor);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_MutexLock(&myMutex);
      sprintf( handleName, "%s.c%d", dbiHandle, tsdPtr->cur_count++ );

      pResultStr = Tcl_NewStringObj( handleName, -1 );

      newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->lmdb_hashtblPtr, handleName, &newvalue);
      Tcl_SetHashValue(newHashEntryPtr, cursor);
      Tcl_MutexUnlock(&myMutex);

      Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) LMDB_CUR,
            (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

  }

  return TCL_OK;
}


static int LMDB_TXN(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
  int choice;
  int result;
  MDB_txn *txn;
  Tcl_HashEntry *hashEntryPtr;
  char *txnHandle;

  ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

  if (tsdPtr->initialized == 0) {
    tsdPtr->initialized = 1;
    tsdPtr->lmdb_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tsdPtr->lmdb_hashtblPtr, TCL_STRING_KEYS);
  }

  static const char *DBTXN_strs[] = {
    "abort",
    "commit",
    "reset",
    "renew",
    "close",
    0
  };

  enum DBTXN_enum {
    DBTXN_ABORT,
    DBTXN_COMMIT,
    DBTXN_RESET,
    DBTXN_RENEW,
    DBTXN_CLOSE,
  };

  if( objc < 2 ){
    Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
    return TCL_ERROR;
  }

  if( Tcl_GetIndexFromObj(interp, objv[1], DBTXN_strs, "option", 0, &choice) ){
    return TCL_ERROR;
  }

  /*
   * Get the MDB_txn * point
   */
  txnHandle = Tcl_GetStringFromObj(objv[0], 0);
  hashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, txnHandle );
  if( !hashEntryPtr ) {
    if( interp ) {
        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
        Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", txnHandle, (char *)NULL );
    }

    return TCL_ERROR;
  }

  txn = Tcl_GetHashValue( hashEntryPtr );

  switch( (enum DBTXN_enum)choice ){

    case DBTXN_ABORT: {
      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      mdb_txn_abort(txn);
      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBTXN_COMMIT: {
      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      result = mdb_txn_commit(txn);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBTXN_RESET: {
      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      mdb_txn_reset(txn);
      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBTXN_RENEW: {
      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      result = mdb_txn_renew(txn);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );

            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBTXN_CLOSE: {
      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      Tcl_MutexLock(&myMutex);
      if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
      Tcl_MutexUnlock(&myMutex);
      Tcl_DeleteCommand(interp, txnHandle);

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

  }

  return TCL_OK;
}


static int LMDB_ENV(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
  int choice;
  int result;
  MDB_env *env;
  Tcl_HashEntry *hashEntryPtr;
  char *handle;

  ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

  if (tsdPtr->initialized == 0) {
    tsdPtr->initialized = 1;
    tsdPtr->lmdb_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tsdPtr->lmdb_hashtblPtr, TCL_STRING_KEYS);
  }

  static const char *DBENV_strs[] = {
    "open",
    "set_mapsize",
    "set_maxreaders",
    "set_maxdbs",
    "sync",
    "stat",
    "copy",
    "get_path",
    "get_maxreaders",
    "get_maxkeysize",
    "close",
    "txn",
    0
  };

  enum DBENV_enum {
    DBENV_OPEN,
    DBENV_SET_MAPSIZE,
    DBENV_SET_MAXREADERS,
    DBENV_SET_MAXDBS,
    DBENV_SYNC,
    DBENV_STAT,
    DBENV_COPY,
    DBENV_GET_PATH,
    DBENV_GET_MAXREADERS,
    DBENV_GET_MAXKEYSIZE,
    DBENV_CLOSE,
    DBENV_TXN,
  };

  if( objc < 2 ){
    Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
    return TCL_ERROR;
  }

  if( Tcl_GetIndexFromObj(interp, objv[1], DBENV_strs, "option", 0, &choice) ){
    return TCL_ERROR;
  }

  /*
   * Get the MDB_env * point
   */
  handle = Tcl_GetStringFromObj(objv[0], 0);
  hashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, handle );
  if( !hashEntryPtr ) {
    if( interp ) {
        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
        Tcl_AppendStringsToObj( resultObj, "invalid env handle ", handle, (char *)NULL );
    }

    return TCL_ERROR;
  }

  env = Tcl_GetHashValue( hashEntryPtr );

  switch( (enum DBENV_enum)choice ){

    case DBENV_OPEN: {
      const char *zArg;
      char *path = NULL;
      mode_t mode = 0664;
      int flags;
      int i = 0;

      flags = MDB_FIXEDMAP;

      if( objc < 4 || (objc&1)!=0 ){
        Tcl_WrongNumArgs(interp, 1, objv,
        "ENV_HANDLE -path path ?-mode mode? ?-fixedmap BOOLEAN? ?-nosubdir BOOLEAN? ?-readonly BOOLEAN? ?-nosync BOOLEAN? ?-nordahead BOOLEAN? "
        );

        return TCL_ERROR;
      }

      for(i=2; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);
        if( strcmp(zArg, "-path")==0 ){
            path = Tcl_GetStringFromObj(objv[i+1], 0);
        } else if( strcmp(zArg, "-mode")==0 ){
            Tcl_GetIntFromObj(interp, objv[i+1], &mode);
        } else if( strcmp(zArg, "-fixedmap")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_FIXEDMAP;
            }else{
              flags &= ~MDB_FIXEDMAP;
            }
        } else if( strcmp(zArg, "-nosubdir")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_NOSUBDIR;
            }else{
              flags &= ~MDB_NOSUBDIR;
            }
        } else if( strcmp(zArg, "-readonly")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_RDONLY;
            }else{
              flags &= ~MDB_RDONLY;
            }
        } else if( strcmp(zArg, "-nosync")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_NOSYNC;
            }else{
              flags &= ~MDB_NOSYNC;
            }
        } else if( strcmp(zArg, "-nordahead")==0 ){
            /*
             * Turn off readahead. Most operating systems perform readahead on
             * read requests by default.
             * Turning it off may help random read performance when the DB is
             * larger than RAM and system RAM is full.
             * The option is not implemented on Windows.
             */
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_NORDAHEAD;
            }else{
              flags &= ~MDB_NORDAHEAD;
            }
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      if(!path) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "empty path string ", (char *)NULL );
        }

        return TCL_ERROR;
      }

      result = mdb_env_open(env, path, flags, mode);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBENV_SET_MAPSIZE: {
      Tcl_WideInt wideValue;

      if( objc == 3 ){
        Tcl_GetWideIntFromObj(interp, objv[2], &wideValue);
      }else{
        Tcl_WrongNumArgs(interp, 2, objv, "size");
        return TCL_ERROR;
      }

      result = mdb_env_set_mapsize(env, (size_t) wideValue);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBENV_SET_MAXREADERS: {
      int value;

      if( objc == 3 ){
        Tcl_GetIntFromObj(interp, objv[2], &value);
      }else{
        Tcl_WrongNumArgs(interp, 2, objv, "nReaders ");
        return TCL_ERROR;
      }

      result = mdb_env_set_maxreaders(env, value);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBENV_SET_MAXDBS: {
      int value;

      if( objc == 3 ){
        Tcl_GetIntFromObj(interp, objv[2], &value);
      }else{
        Tcl_WrongNumArgs(interp, 2, objv, "nDbs");
        return TCL_ERROR;
      }

      result = mdb_env_set_maxdbs(env, value);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBENV_SYNC: {
      int value;

      if( objc == 3 ){
        Tcl_GetIntFromObj(interp, objv[2], &value);
      }else{
        Tcl_WrongNumArgs(interp, 2, objv, "force");
        return TCL_ERROR;
      }

      /*
       * Data is always written to disk when mdb_txn_commit() is called, but
       * the operating system may keep it buffered. LMDB always flushes the
       * OS buffers upon commit as well, unless the environment was opened
       * with MDB_NOSYNC or in part MDB_NOMETASYNC. This call is not valid
       * if the environment was opened with MDB_RDONLY.
       */
      if(value) {  //If value is non-zero, force a synchronous flush.
        result = mdb_env_sync(env, value);
        if(result != 0) {
          if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
          }

          return TCL_ERROR;
        }
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBENV_STAT: {
      MDB_stat stat;
      Tcl_Obj *pResultStr = NULL;

      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      result = mdb_env_stat(env, &stat);
      if(result != 0) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
          Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      pResultStr = Tcl_NewListObj(0, NULL);
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_psize));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_depth));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_branch_pages));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_leaf_pages));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_overflow_pages));
      Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(stat.ms_entries));

      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

    case DBENV_COPY: {
      char *path;
      char *zArg;
      int len;
      #if MDB_VERSION_MAJOR > 0 || \
        (MDB_VERSION_MAJOR == 0 && (MDB_VERSION_MINOR > 9 || \
                    (MDB_VERSION_MINOR == 9 && \
                     MDB_VERSION_PATCH > 13)))
      int flags = 0;
      #endif

      if( objc == 3 || objc == 5){
        path = Tcl_GetStringFromObj(objv[2], &len);
        if( !path || len < 1 ){
            return TCL_ERROR;
        }

      #if MDB_VERSION_MAJOR > 0 || \
        (MDB_VERSION_MAJOR == 0 && (MDB_VERSION_MINOR > 9 || \
                    (MDB_VERSION_MINOR == 9 && \
                     MDB_VERSION_PATCH > 13)))
        if(objc == 5) {
          zArg = Tcl_GetStringFromObj(objv[3], 0);
          if( strcmp(zArg, "-cp_compact")==0 ){
              int b;
              if( Tcl_GetBooleanFromObj(interp, objv[4], &b) ) return TCL_ERROR;
              if( b ){
                flags |= MDB_CP_COMPACT ;
              }else{
                flags &= ~MDB_CP_COMPACT;
              }
          } else{
             Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
             return TCL_ERROR;
          }
        }
        #endif
      }else{
        Tcl_WrongNumArgs(interp, 2, objv, "path ?-cp_compact boolean?");
        return TCL_ERROR;
      }

      #if MDB_VERSION_MAJOR > 0 || \
        (MDB_VERSION_MAJOR == 0 && (MDB_VERSION_MINOR > 9 || \
                    (MDB_VERSION_MINOR == 9 && \
                     MDB_VERSION_PATCH > 13)))
      result = mdb_env_copy2(env, path, flags);
      #else
      result = mdb_env_copy(env, path);
      #endif
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

      break;
    }

    case DBENV_GET_PATH: {
      const char *path;
      Tcl_Obj *pResultStr = NULL;

      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      result = mdb_env_get_path(env, &path);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      pResultStr = Tcl_NewStringObj( path, -1 );
      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

    case DBENV_GET_MAXREADERS: {
      unsigned int readers;
      Tcl_Obj *pResultStr = NULL;

      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      result = mdb_env_get_maxreaders(env, &readers);
      if(result != 0) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      pResultStr = Tcl_NewIntObj( readers );
      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

    case DBENV_GET_MAXKEYSIZE: {
      unsigned int maxkeysize;
      Tcl_Obj *pResultStr = NULL;

      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      maxkeysize = mdb_env_get_maxkeysize(env);

      pResultStr = Tcl_NewIntObj( maxkeysize );
      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

    case DBENV_CLOSE: {
      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      mdb_env_close(env);
      Tcl_MutexLock(&myMutex);
      if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
      Tcl_MutexUnlock(&myMutex);
      Tcl_DeleteCommand(interp, handle);

      break;
    }

    case DBENV_TXN: {
      char *zArg;
      char *parentTxn = NULL;
      int len = 0;
      MDB_txn *parent = NULL;
      int flags = 0;
      MDB_txn *txn;
      Tcl_HashEntry *txnHashEntryPtr;
      Tcl_HashEntry *newHashEntryPtr;
      char handleName[16 + TCL_INTEGER_SPACE];
      Tcl_Obj *pResultStr = NULL;
      int newvalue;
      int i = 0;

      if( objc < 2 || (objc&1)!=0 ){
        Tcl_WrongNumArgs(interp, 2, objv, "?-parent txnid? ?-readonly boolean? ");
        return TCL_ERROR;
      }

      for(i=2; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);

        if( strcmp(zArg, "-parent")==0 ){
            parentTxn = Tcl_GetStringFromObj(objv[i+1], &len);
            if( !parentTxn || len < 1 ){
              return TCL_ERROR;
            }
        } else if( strcmp(zArg, "-readonly")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_RDONLY ;
            }else{
              flags &= ~MDB_RDONLY;
            }
        } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      if(parentTxn) {
        txnHashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, parentTxn );
        if( !txnHashEntryPtr ) {
          if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid txn handle ", parentTxn, (char *)NULL );
          }

          return TCL_ERROR;
        }

        parent = Tcl_GetHashValue( txnHashEntryPtr );
      }

      result = mdb_txn_begin(env, parent, flags, &txn);
      if(result != 0) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
          Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_MutexLock(&myMutex);
      sprintf( handleName, "%s.txn%d", handle, tsdPtr->txn_count++ );

      pResultStr = Tcl_NewStringObj( handleName, -1 );

      newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->lmdb_hashtblPtr, handleName, &newvalue);
      Tcl_SetHashValue(newHashEntryPtr, txn);
      Tcl_MutexUnlock(&myMutex);

      Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) LMDB_TXN,
            (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

  }

  return TCL_OK;
}


static int LMDB_MAIN(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
  int choice;
  int result;

  ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

  if (tsdPtr->initialized == 0) {
    tsdPtr->initialized = 1;
    tsdPtr->lmdb_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tsdPtr->lmdb_hashtblPtr, TCL_STRING_KEYS);
  }

  static const char *DB_strs[] = {
    "env",
    "open",
    "version",
    0
  };

  enum DB_enum {
    DB_ENV,
    DB_OPEN,
    DB_VERSION,
  };

  if( objc < 2 ){
    Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
    return TCL_ERROR;
  }

  if( Tcl_GetIndexFromObj(interp, objv[1], DB_strs, "option", 0, &choice) ){
    return TCL_ERROR;
  }

  switch( (enum DB_enum)choice ){

    case DB_ENV: {
      MDB_env *env;
      Tcl_HashEntry *hashEntryPtr;
      char handleName[16 + TCL_INTEGER_SPACE];
      Tcl_Obj *pResultStr = NULL;
      int newvalue;

      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      result = mdb_env_create (&env);
      if(result != 0) {
        if( interp ) {
          Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
          Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
        }

        return TCL_ERROR;
      }

      Tcl_MutexLock(&myMutex);
      sprintf( handleName, "env%d", tsdPtr->env_count++ );

      pResultStr = Tcl_NewStringObj( handleName, -1 );

      hashEntryPtr = Tcl_CreateHashEntry(tsdPtr->lmdb_hashtblPtr, handleName, &newvalue);
      Tcl_SetHashValue(hashEntryPtr, env);
      Tcl_MutexUnlock(&myMutex);

      Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) LMDB_ENV,
            (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

    case DB_OPEN: {
      char *zArg;
      MDB_env *env;
      Tcl_HashEntry *hashEntryPtr;
      char *handle = NULL;
      MDB_txn *txn;
      MDB_dbi dbi;
      const char *database = NULL;
      int len;
      int flags = 0;
      Tcl_HashEntry *newHashEntryPtr;
      char handleName[16 + TCL_INTEGER_SPACE];
      Tcl_Obj *pResultStr = NULL;
      int newvalue;
      int i = 0;

      if( objc < 4 || (objc&1)!=0 ){
          Tcl_WrongNumArgs(interp, 2, objv,
          "-env env_handle ?-name database? ?-reversekey BOOLEAN? ?-dupsort BOOLEAN? ?-dupfixed BOOLEAN? ?-reversedup BOOLEAN? ?-create BOOLEAN? "
          );

        return TCL_ERROR;
      }

      for(i=2; i+1<objc; i+=2){
        zArg = Tcl_GetString(objv[i]);

        if( strcmp(zArg, "-name")==0 ){
            database = Tcl_GetStringFromObj(objv[i+1], &len);
            if(!database || len < 0) {
                return TCL_ERROR;
            }
        } else if( strcmp(zArg, "-env")==0 ){
            handle = Tcl_GetStringFromObj(objv[i+1], &len);
            if(!handle || len < 0) {
                return TCL_ERROR;
            }
        } else if( strcmp(zArg, "-reversekey")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_REVERSEKEY;
            }else{
              flags &= ~MDB_REVERSEKEY;
            }
        } else if( strcmp(zArg, "-dupsort")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_DUPSORT;
            }else{
              flags &= ~MDB_DUPSORT;
            }
        } else if( strcmp(zArg, "-dupfixed")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_DUPFIXED;
            }else{
              flags &= ~MDB_DUPFIXED;
            }
        } else if( strcmp(zArg, "-reversedup")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_REVERSEDUP;
            }else{
              flags &= ~MDB_REVERSEDUP;
            }
        } else if( strcmp(zArg, "-create")==0 ){
            int b;
            if( Tcl_GetBooleanFromObj(interp, objv[i+1], &b) ) return TCL_ERROR;
            if( b ){
              flags |= MDB_CREATE;
            }else{
              flags &= ~MDB_CREATE;
            }
          } else{
           Tcl_AppendResult(interp, "unknown option: ", zArg, (char*)0);
           return TCL_ERROR;
        }
      }

      if(!handle) {
          if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid env handle ", (char *)NULL );
          }

          return TCL_ERROR;
      }

      hashEntryPtr = Tcl_FindHashEntry( tsdPtr->lmdb_hashtblPtr, handle );
      if( !hashEntryPtr ) {
          if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid env handle ", handle, (char *)NULL );
          }

          return TCL_ERROR;
      }

      env = Tcl_GetHashValue( hashEntryPtr );
      result = mdb_txn_begin(env, NULL, 0, &txn);
      if(result != 0) {
          if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
          }

          return TCL_ERROR;
      }

      result = mdb_dbi_open(txn, database, flags, &dbi);
      if(result != 0) {
          if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
          }

          return TCL_ERROR;
      }

      /*
       * The database handle will be private to the current transaction
       * until the transaction is successfully committed.
       * After a successful commit the handle will reside in the shared
       * environment, and may be used by other transactions.
       * Here I use a transaction and try to commit, users need use
       * env_handle txn command (at script level) to create a new
       * transactions.
       */
      result = mdb_txn_commit(txn);
      if(result != 0) {
          if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "ERROR: ", mdb_strerror(result), (char *)NULL );
          }

          return TCL_ERROR;
      }

      Tcl_MutexLock(&myMutex);
      sprintf( handleName, "dbi%d", tsdPtr->dbi_count++ );

      pResultStr = Tcl_NewStringObj( handleName, -1 );

      newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->lmdb_hashtblPtr, handleName, &newvalue);
      Tcl_SetHashValue(newHashEntryPtr, dbi);
      Tcl_MutexUnlock(&myMutex);

      Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) LMDB_DBI,
            (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

      Tcl_SetObjResult(interp, pResultStr);

      break;
    }

    case DB_VERSION: {
      char *zArg;
      char *verString = NULL;
      int major = 0, minor = 0, patch = 0;
      int bReturnString = 0;
      Tcl_Obj *pResultStr = NULL;

      if( objc == 2 || objc == 3 ){
        if (objc == 3) {
          zArg = Tcl_GetStringFromObj(objv[2], 0);
          if( strcmp(zArg, "-string")==0 ){
            bReturnString = 1;
          } else {
            Tcl_WrongNumArgs(interp, 2, objv, "?-string?");
            return TCL_ERROR;
          }
        }
      }else{
        Tcl_WrongNumArgs(interp, 2, objv, "?-string?");
        return TCL_ERROR;
      }

      verString = mdb_version(&major, &minor, &patch);

      if(bReturnString) {
          pResultStr = Tcl_NewStringObj(verString, -1);
      } else {
          pResultStr = Tcl_NewListObj(0, NULL);
          Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(major));
          Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(minor));
          Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(patch));
      }

      Tcl_SetObjResult(interp,  pResultStr);

      break;
    }
  }

  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Lmdb_Init --
 *
 *	Initialize the new package.
 *
 * Results:
 *	A standard Tcl result
 *
 * Side effects:
 *	The Lmdb package is created.
 *
 *----------------------------------------------------------------------
 */

EXTERN int Lmdb_Init(Tcl_Interp *interp)
{
    /*
     * This may work with 8.0, but we are using strictly stubs here,
     * which requires 8.1.
     */
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION) != TCL_OK) {
	return TCL_ERROR;
    }

    //Tcllmdb_InitHashTable();

    /*
     *   Tcl_GetThreadData handles the auto-initialization of all data in
     *  the ThreadSpecificData to NULL at first time.
     */
    Tcl_MutexLock(&myMutex);
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->lmdb_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->lmdb_hashtblPtr, TCL_STRING_KEYS);

        tsdPtr->env_count = 0;
        tsdPtr->txn_count = 0;
        tsdPtr->dbi_count = 0;
        tsdPtr->cur_count = 0;
    }
    Tcl_MutexUnlock(&myMutex);

    /* Add a thread exit handler to delete hash table */
    Tcl_CreateThreadExitHandler(LMDB_Thread_Exit, (ClientData)NULL);

    Tcl_CreateObjCommand(interp, "lmdb", (Tcl_ObjCmdProc *) LMDB_MAIN,
            (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}
