#include "quickjs.h"
#include "v8.h"
#include<cstring>

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

namespace v8 {
namespace platform {

std::unique_ptr<v8::Platform> NewDefaultPlatform() {
    return std::unique_ptr<v8::Platform>{};
}

}  // namespace platform
}  // namespace v8

namespace v8 {

Maybe<uint32_t> Value::Uint32Value(Local<Context> context) const {
    if (jsValue_) {
        int tag = JS_VALUE_GET_TAG(u_.value_);
        if (tag == JS_TAG_INT) {
            return Maybe<uint32_t>((uint32_t)JS_VALUE_GET_INT(u_.value_));
        }
        else {
            return Maybe<uint32_t>((uint32_t)JS_VALUE_GET_FLOAT64(u_.value_));
        }
    }
    else {
        return Maybe<uint32_t>();
    }
}
    
Maybe<int32_t> Value::Int32Value(Local<Context> context) const {
    if (jsValue_) {
        int tag = JS_VALUE_GET_TAG(u_.value_);
        if (tag == JS_TAG_INT) {
            return Maybe<int32_t>((int32_t)JS_VALUE_GET_INT(u_.value_));
        }
        else {
            return Maybe<int32_t>((int32_t)JS_VALUE_GET_FLOAT64(u_.value_));
        }
    }
    else {
        return Maybe<int32_t>();
    }
}
    
bool Value::IsUndefined() const {
    return jsValue_ && JS_IsUndefined(u_.value_);
}

bool Value::IsNull() const {
    return jsValue_ && JS_IsNull(u_.value_);
}

bool Value::IsNullOrUndefined() const {
    return jsValue_ && (JS_IsUndefined(u_.value_) || JS_IsNull(u_.value_));
}

bool Value::IsString() const {
    return !jsValue_ || JS_IsString(u_.value_);
}

bool Value::IsSymbol() const {
    return jsValue_ && JS_IsSymbol(u_.value_);
}

Isolate::Isolate() : current_context_(nullptr) {
    runtime_ = JS_NewRuntime();
};

Isolate::~Isolate() {
    JS_FreeRuntime(runtime_);
};

Context::Context(Isolate* isolate) :isolate_(isolate) {
    context_ = JS_NewContext(isolate->runtime_);
    JS_SetContextOpaque(context_, isolate);
}

//TODO: 哪里搞ctx？
//bool Value::IsFunction() const {
    //return jsValue_ && JS_IsFunction(<#JSContext *ctx#>, <#JSValue val#>)
    //if (!jsValue_) return false;
    //if (JS_VALUE_GET_TAG(u_.value_) != JS_TAG_OBJECT)
    //    return false;
    //JSObject *p = JS_VALUE_GET_OBJ(u_.value_);
    //return p->class_id
//}

bool Value::IsObject() const {
    return jsValue_ && JS_IsObject(u_.value_);
}

bool Value::IsBigInt() const {
    return jsValue_ && JS_VALUE_GET_TAG(u_.value_) == JS_TAG_BIG_INT;
}

bool Value::IsBoolean() const {
    return jsValue_ && JS_IsBool(u_.value_);
}

bool Value::IsNumber() const {
    return jsValue_ && JS_IsNumber(u_.value_);
}

bool Value::IsExternal() const {
    return jsValue_ && JS_VALUE_GET_TAG(u_.value_) == JS_TAG_EXTERNAL;
}


MaybeLocal<String> String::NewFromUtf8(
    Isolate* isolate, const char* data,
    NewStringType type, int length) {
    auto str = new String();
    //printf("NewFromUtf8:%p\n", str);
    str->u_.str_.data_ = const_cast<char*>(data);
    str->u_.str_.len_ = length > 0 ? length : strlen(data);
    str->jsValue_ = false;
    return Local<String>(str);
}

//！！如果一个Local<String>用到这个接口了，就不能再传入JS
MaybeLocal<Script> Script::Compile(
    Local<Context> context, Local<String> source,
    ScriptOrigin* origin) {
    Script* script = new Script();
    script->source_ = source;
    if (origin) {
        script->resource_name_ = MaybeLocal<String>(Local<String>::Cast(origin->resource_name_));
    }
    return MaybeLocal<Script>(Local<Script>(script));
}

MaybeLocal<Value> Script::Run(Local<Context> context) {
    auto isolate = context->GetIsolate();
    Isolate::Scope isolateScope(isolate);
    HandleScope handleScope(isolate);
    Context::Scope contextScope(context);

    String::Utf8Value source(isolate, source_);
    auto ret = JS_Eval(context->context_, *source, source.length(), resource_name_.IsEmpty() ? "eval" : *String::Utf8Value(isolate, resource_name_.ToLocalChecked()), JS_EVAL_FLAG_STRICT);

    if (JS_IsException(ret)) {
        return MaybeLocal<Value>();
    } else {
        Value* val = new Value();
        val->u_.value_ = ret;
        return MaybeLocal<Value>(Local<Value>(val));
    }
}

Script::~Script() {
    //JS_FreeValue(context_->context_, source_->value_);
    //if (!resource_name_.IsEmpty()) {
    //    JS_FreeValue(context_->context_, resource_name_.ToLocalChecked()->value_);
    //}
}

Local<External> External::New(Isolate* isolate, void* value) {
    External* external = new External();
    external->u_.value_ = JS_MKPTR(JS_TAG_EXTERNAL, value);
    return Local<External>(external);
}

void* External::Value() const {
    return JS_VALUE_GET_PTR(u_.value_);
}

double Number::Value() const {
    return JS_VALUE_GET_FLOAT64(u_.value_);
}

Local<Number> Number::New(Isolate* isolate, double value) {
    Number* ret = new Number();
    ret->u_.value_ = JS_NewFloat64(isolate->GetCurrentContext()->context_, value);
    return Local<Number>(ret);
}

Local<Integer> Integer::New(Isolate* isolate, int32_t value) {
    Integer* ret = new Integer();
    ret->u_.value_ = JS_MKVAL(JS_TAG_INT, value);
    return Local<Integer>(ret);
}

bool Boolean::Value() const {
    return JS_VALUE_GET_BOOL(u_.value_);
}

Local<Boolean> Boolean::New(Isolate* isolate, bool value) {
    Boolean* ret = new Boolean();
    ret->u_.value_ = JS_MKVAL(JS_TAG_BOOL, (value != 0));
    return Local<Boolean>(ret);
}

Local<Integer> Integer::NewFromUnsigned(Isolate* isolate, uint32_t value) {
    Integer* ret = new Integer();
    ret->u_.value_ = JS_NewUint32(isolate->GetCurrentContext()->context_, value);;
    return Local<Integer>(ret);
}

int64_t Integer::Value() const {
    if (JS_VALUE_GET_TAG(u_.value_) == JS_TAG_INT) {
        return JS_VALUE_GET_INT(u_.value_);
    } else if (JS_VALUE_GET_TAG(u_.value_) == JS_TAG_FLOAT64) {
        return (int64_t)JS_VALUE_GET_FLOAT64(u_.value_);
    } else {
        return 0;
    }
}

String::Utf8Value::Utf8Value(Isolate* isolate, Local<v8::Value> obj) {
    auto context = isolate->GetCurrentContext();
    Local<String> localStr = Local<String>::Cast(obj);
    if (localStr->jsValue_) {
        data_ = const_cast<char*>(JS_ToCStringLen(context->context_, &len_, localStr->u_.value_));
        context_ = context->context_;
    } else {
        data_ = localStr->u_.str_.data_;
        len_ = localStr->u_.str_.len_;
    }
}

String::Utf8Value::~Utf8Value() {
    if (context_) {
        JS_FreeCString(context_, data_);
    }
}

int Isolate::RegFunctionTemplate(Local<FunctionTemplate> data) {
    function_templates_.push_back(data);
    return function_templates_.size() - 1;
}

Local<FunctionTemplate>& Isolate::GetFunctionTemplate(int index) {
    return function_templates_[index];
}

Local<Object> Context::Global() {
    Object* obj = new Object();
    obj->u_.value_ = JS_GetGlobalObject(context_);
    return Local<Object>(obj);
}

Context::~Context() {
    JS_FreeContext(context_);
}

Local<FunctionTemplate> FunctionTemplate::New(Isolate* isolate, FunctionCallback callback,
                                              Local<Value> data) {
    Local<FunctionTemplate> functionTemplate(new FunctionTemplate());
    functionTemplate->callback_ = callback;
    functionTemplate->data_ = data;
    functionTemplate->magic_ = isolate->RegFunctionTemplate(functionTemplate);
    return functionTemplate;
}

MaybeLocal<Function> FunctionTemplate::GetFunction(Local<Context> context) {
    JSValue func = JS_NewCFunctionMagic(context->context_, [](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
        JSValue ret = JS_UNDEFINED;
        Isolate* isolate = reinterpret_cast<Isolate*>(JS_GetContextOpaque(ctx));
        Local<FunctionTemplate> functionTemplate = isolate->GetFunctionTemplate(magic);
        FunctionCallbackInfo<Value> callbackInfo;
        callbackInfo.isolate_ = isolate;
        callbackInfo.argc_ = argc;
        callbackInfo.argv_ = argv;
        callbackInfo.context_ = ctx;
        callbackInfo.this_ = this_val;
        callbackInfo.magic_ = magic;
        callbackInfo.value_ = JS_UNDEFINED;
        
        functionTemplate->callback_(callbackInfo);
        
        return callbackInfo.value_;
    }, "native", 0, JS_CFUNC_generic_magic, magic_);
    
    Function* function = new Function();
    function->u_.value_ = func;
    
    return MaybeLocal<Function>(Local<Function>(function));
}

Maybe<bool> Object::Set(Local<Context> context,
                        Local<Value> key, Local<Value> value) {
    JSValue jsValue;
    if (value->jsValue_) {
        jsValue = value->u_.value_;
    } else {
        jsValue = JS_NewStringLen(context->context_, value->u_.str_.data_, value->u_.str_.len_);
    }
    if (!key->jsValue_) {
        JS_SetPropertyStr(context->context_, u_.value_, key->u_.str_.data_, jsValue);
    } else {
        if (key->IsNumber()) {
            JS_SetPropertyUint32(context->context_, u_.value_, key->Uint32Value(context).ToChecked(), jsValue);
        } else {
            JS_SetProperty(context->context_, u_.value_, JS_ValueToAtom(context->context_, key->u_.value_), jsValue);
        }
    }
    
    return Maybe<bool>(true);
}

}  // namespace v8
