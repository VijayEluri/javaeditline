#include <jni.h>
#include <stdio.h>
#include <histedit.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "org_clapper_editline_EditLine.h"

#define min(a, b)  ((a) < (b) ? (a) : (b))

#define PROMPT_MAX 128
#define elPointer2jlong(handle) ((jlong) ((long) handle)) 
#define jlong2elPointer(jl) ((EditLine *) ((long) jl))

typedef struct _SimpleStringList
{
    const char *string;
    struct _SimpleStringList *next;
}
SimpleStringList;

typedef struct _jEditLineData
{
    char prompt[PROMPT_MAX];
    History *history;
    JNIEnv *env;
    jclass javaClass;
    jobject javaEditLine;
    jmethodID handleCompletionMethodID;
}
jEditLineData;

static jEditLineData *get_data(EditLine *el)
{
    void *d;
    el_get(el, EL_CLIENTDATA, &d);
    return (jEditLineData *) d;
}

static void set_prompt(EditLine *el, const char *new_prompt)
{
    jEditLineData *data = get_data(el);
    strncpy(data->prompt, new_prompt, PROMPT_MAX - 1);
}

static const char *get_prompt(EditLine *el)
{
    jEditLineData *data = get_data(el);
    return data->prompt;
}

static unsigned char complete(EditLine *el, int ch)
{
    jEditLineData *data = get_data(el);
    JNIEnv *env = data->env;

    const LineInfo *lineInfo = el_line(el);

    jstring jLine;
    jint jCursor;
    jstring jToken;
    int tokenLength = 0;
    
    if (lineInfo != NULL)
    {
        jLine = (*env)->NewStringUTF(env, lineInfo->buffer);

        /* Find the beginning of the current token */

        const char *ptr;
	for (ptr = lineInfo->cursor - 1;
             (!isspace((unsigned char)*ptr)) && (ptr > lineInfo->buffer); ptr--)
		continue;
        if (ptr != lineInfo->buffer)
        {
            /* Stopped on white space. Increment to get to start of token. */
            ptr++;
        }

	tokenLength = lineInfo->cursor - ptr + 1;

        /* Save it as a Java string. */

        if (tokenLength == 0)
            jToken = (*env)->NewStringUTF(env, "");

        else
        {
            char *token = (char *) malloc(tokenLength + 1);
            strncpy(token, ptr, tokenLength);
            jToken = (*env)->NewStringUTF(env, token);
            free(token);
        }

        jCursor = (long) (lineInfo->cursor - lineInfo->buffer);
    }

    else
    {
        jToken = (*env)->NewStringUTF(env, "");
        jLine = (*env)->NewStringUTF(env, "");
        jCursor = 0;
    }

    jmethodID method = data->handleCompletionMethodID;

    unsigned char result = CC_ERROR;
    jstring completion = (*env)->CallObjectMethod(env,
                                                  data->javaEditLine,
                                                  method,
                                                  jToken,
                                                  jLine,
                                                  jCursor);

    if (completion != NULL)
    {
        const char *str = (*env)->GetStringUTFChars(env, completion, NULL);
        if (str == NULL)
        {
            puts("Out of memory (Java) during completion.");
        }

        else
        {
            result = CC_REFRESH;
            el_deletestr(el, tokenLength - 1);
            el_insertstr(el, str);
            (*env)->ReleaseStringUTFChars(env, completion, str);
        }
    }

    return result;
}

/*
 * Class:  org_clapper_editline_EditLine
 * Method: static long n_el_init(String program, EditLine javaEditLine)
 */
JNIEXPORT jlong JNICALL Java_org_clapper_editline_EditLine_n_1el_1init
    (JNIEnv *env, jclass cls, jstring program, jobject javaEditLine)
{
    const char *cProgram = (*env)->GetStringUTFChars(env, program, NULL);
    if (cProgram == NULL)
    {
        /* OutOfMemoryError already thrown */
        return;
    }

    jEditLineData *data = (jEditLineData*) malloc(sizeof(jEditLineData));
    jlong handle = 0;

    if (data == NULL)
    {
        jclass exc = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
        (*env)->ThrowNew(env, exc, "unable to allocation jEditLineData");
    }

    else
    {
        EditLine *el = el_init(cProgram, stdin, stdout, stderr);
        data->history = history_init();

        data->env = env;
        data->javaClass = (*env)->NewGlobalRef(env, cls);
        data->javaEditLine = (*env)->NewGlobalRef(env, javaEditLine);
        data->handleCompletionMethodID = (*env)->GetMethodID(
            env, cls,
            "handleCompletion",
            "(Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;");

        if (data->handleCompletionMethodID != NULL)
            el_set(el, EL_ADDFN, "ed-complete", "Complete", complete);

        el_set(el, EL_CLIENTDATA, (void *) data);
        el_set(el, EL_PROMPT, get_prompt);
        el_set(el, EL_HIST, history, data->history);
        el_set(el, EL_SIGNAL, 1);
        handle = elPointer2jlong(el);
    }

    (*env)->ReleaseStringUTFChars(env, program, cProgram);
    return handle;
}

/*
 * Class:  org_clapper_editline_EditLine
 * Method: static void n_el_source(long handle, String path)
 */
JNIEXPORT void JNICALL Java_org_clapper_editline_EditLine_n_1el_1source
    (JNIEnv *env, jclass cls, jlong handle, jstring javaPath)
{
    EditLine *el = jlong2elPointer(handle);
    if (javaPath == NULL)
        el_source(el, NULL);

    else
    {
        const char *path = (*env)->GetStringUTFChars(env, javaPath, NULL);
        if (path == NULL)
        {
            /* OutOfMemoryError already thrown */
        }

        else
        {
            el_source(el, path);
            (*env)->ReleaseStringUTFChars(env, javaPath, path);
        }
    }
}


/*
 * Class:  org_clapper_editline_EditLine
 * Method: static void n_el_end(long handle)
 */
JNIEXPORT void JNICALL Java_org_clapper_editline_EditLine_n_1el_1end
    (JNIEnv *env, jclass cls, jlong handle)
{
    EditLine *el = jlong2elPointer(handle);
    jEditLineData *data = get_data(el);
    history_end(data->history);
    el_end(el);
    el = NULL;
    free(data);
}

/*
 * Class:  org_clapper_editline_EditLine
 * Method: static void n_el_set_prompt(long handle, String prompt)
 */
JNIEXPORT void JNICALL Java_org_clapper_editline_EditLine_n_1el_1set_1prompt
    (JNIEnv *env, jclass cls, jlong handle, jstring prompt)
{
    const char *str = (*env)->GetStringUTFChars(env, prompt, NULL);
    if (str == NULL)
    {
        /* OutOfMemoryError already thrown */
    }

    else
    {
        EditLine *el = jlong2elPointer(handle);
        set_prompt(el, str);
        (*env)->ReleaseStringUTFChars(env, prompt, str);
    }
}

/*
 * Class:  org_clapper_editline_EditLine
 * Method: static String n_el_gets(long handle)
 */
JNIEXPORT jstring JNICALL Java_org_clapper_editline_EditLine_n_1el_1gets
    (JNIEnv *env, jclass cls, jlong handle)
{
    jstring result = NULL;
    EditLine *el = jlong2elPointer(handle);
    int count;
    const char *line = el_gets(el, &count);
    if (line != NULL)
        result = (*env)->NewStringUTF(env, line);

    return result;
}

/*
 * Class:  org_clapper_editline_EditLine
 * Method: static int n_history_get_size(long handle)
 */
JNIEXPORT jint JNICALL Java_org_clapper_editline_EditLine_n_1history_1get_1size
    (JNIEnv *env, jclass cls, jlong handle)
{
    EditLine *el = jlong2elPointer(handle);
    jEditLineData *data = get_data(el);
    HistEvent ev;
    /**
       H_GETSIZE doesn't seem to work on the Mac.

    int result = (jint) history(data->history, &ev, H_GETSIZE);
    */
    int total = 0;
    int rc;
    for (rc = history(data->history, &ev, H_LAST);
         rc != -1;
         rc = history(data->history, &ev, H_PREV))
        total++;

    return total;
}

/*
 * Class:  org_clapper_editline_EditLine
 * Method: static void n_history_set_size(long handle, int size)
 */
JNIEXPORT void JNICALL Java_org_clapper_editline_EditLine_n_1history_1set_1size
    (JNIEnv *env, jclass cls, jlong handle, jint newSize)
{
    EditLine *el = jlong2elPointer(handle);
    jEditLineData *data = get_data(el);
    HistEvent ev;
    history(data->history, &ev, H_SETSIZE, (int) newSize);
}

/*
 * Class:  org_clapper_editline_EditLine
 * Method: static void n_history_clear(long handle)
 */
JNIEXPORT void JNICALL Java_org_clapper_editline_EditLine_n_1history_1clear
    (JNIEnv *env, jclass cls, jlong handle)
{
    EditLine *el = jlong2elPointer(handle);
    HistEvent ev;
    jEditLineData *data = get_data(el);
    history(data->history, &ev, H_CLEAR);
}

/*
 * Class:  org_clapper_editline_EditLine
 * Method: static void n_history_append(long handle, String line)
 */
JNIEXPORT void JNICALL Java_org_clapper_editline_EditLine_n_1history_1append
    (JNIEnv *env, jclass cls, jlong handle, jstring line)
{
    const char *str = (*env)->GetStringUTFChars(env, line, NULL);
    if (str == NULL)
    {
        /* OutOfMemoryError already thrown */
    }

    else
    {
    EditLine *el = jlong2elPointer(handle);
        HistEvent ev;
        ev.str = str;
        jEditLineData *data = get_data(el);
        history(data->history, &ev, H_ENTER, str);
        (*env)->ReleaseStringUTFChars(env, line, str);
    }
}

/*
 * Class:  org_clapper_editline_EditLine
 * Method: static String[] n_history_get_all(long handle)
 */
JNIEXPORT
jobjectArray JNICALL Java_org_clapper_editline_EditLine_n_1history_1get_1all
    (JNIEnv *env, jclass cls, jlong handle)
{
    SimpleStringList *list = NULL;

    int total = 0;
    int rc;
    HistEvent ev;

    /*
      Capture the history elements in a linked-list, so we only have to
      traverse the list once. For simplicity, insert the entries at the top
      (like a stack).
    */
    EditLine *el = jlong2elPointer(handle);
    jEditLineData *data = get_data(el);
    for (rc = history(data->history, &ev, H_LAST);
         rc != -1;
         rc = history(data->history, &ev, H_PREV))
    {
        SimpleStringList *entry =
            (SimpleStringList *) malloc(sizeof(SimpleStringList));

        if (list == NULL)
        {
            list = entry;
            entry->next = NULL;
        }
        else
        {
            entry->next = list;
            list = entry;
        }

        entry->string = strdup(ev.str);
        total++;
    }

    /* Allocate an appropriate size array. */

    jobjectArray result = (jobjectArray)
        (*env)->NewObjectArray(env, total,
                               (*env)->FindClass(env, "java/lang/String"),
                               (*env)->NewStringUTF(env, ""));

    /*
      Traverse the list and fill the array backwards, since the list
      is in reverse order.
    */
    int i = total;
    SimpleStringList *entry = list;
    SimpleStringList *prev = NULL;
    while (entry != NULL)
    {
        i--;
        assert (i >= 0);
        assert(entry->string != NULL);

        /* Create a Java version of the string. */
        jstring js = (*env)->NewStringUTF(env, entry->string);

        /* Store it in the Java array. */
        (*env)->SetObjectArrayElement(env, result, i, js);

        /* Move along. */
        prev = entry;
        entry = entry->next;

        /* Free the linked list entry and its string. */
        free((void *) prev->string);
        free(prev);
    }

    return result;
}

/*
 * Class:     org_clapper_editline_EditLine
 * Method:    static void n_history_set_unique(long handle, boolean on)
 * Signature: (Z)V
 */
JNIEXPORT void
JNICALL Java_org_clapper_editline_EditLine_n_1history_1set_1unique
    (JNIEnv *env, jclass cls, jlong handle, jboolean on)
{
    EditLine *el = jlong2elPointer(handle);
    jEditLineData *data = get_data(el);
    HistEvent ev;
    history(data->history, &ev, H_SETUNIQUE, on ? 1 : 0);
}