#include "quickjs.h"

JSValue JS_NewFloat64_(JSContext *ctx, double d) {
    return JS_NewFloat64(ctx, d);
}

JSValue JS_NewStringLen_(JSContext *ctx, const char *str1, size_t len1) {
    return JS_NewStringLen(ctx, str1, len1);
}

JSValue JS_NewInt32_(JSContext *ctx, int32_t val) {
    return JS_NewInt32(ctx, val);
}

JSValue JS_NewUint32_(JSContext *ctx, uint32_t val) {
    return JS_NewUint32(ctx, val);
}

JSValue JS_True() {
    return JS_TRUE;
}

JSValue JS_False() {
    return JS_FALSE;
}

JSValue JS_Null() {
    return JS_NULL;
}

JSValue JS_Undefined() {
    return JS_UNDEFINED;
}

