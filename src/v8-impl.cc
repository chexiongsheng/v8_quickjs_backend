#include "v8.h"
#include<cstring>

enum
{
    JS_ATOM_NULL_,
#define DEF(name, str) JS_ATOM_##name,
#include "quickjs-atom.h"
#undef DEF
    JS_ATOM_END,
};


#if !defined(CONFIG_CHECK_JSVALUE) && defined(JS_NAN_BOXING)
#define JS_INITVAL(s, t, val) s = JS_MKVAL(t, val)
#define JS_INITPTR(s, t, p) s = JS_MKPTR(t, p)
#else
#define JS_INITVAL(s, t, val) s.tag = t, s.u.int32=val
#define JS_INITPTR(s, t, p) s.tag = t, s.u.ptr = p
#endif

namespace v8 {
namespace platform {

std::unique_ptr<v8::Platform> NewDefaultPlatform() {
    return std::unique_ptr<v8::Platform>{};
}

}  // namespace platform
}  // namespace v8

namespace v8 {

JSValue V8::NewCString(const char* str, size_t len) {
    JSValue ret;
    CString* cstr = (CString*)std::malloc(sizeof(CString) + len);
    cstr->len = len;
    std::strncpy(&cstr->data[0], str, len);
    cstr->data[len] = '\0';
    JS_INITPTR(ret, JS_TAG_CSTRING, cstr);
    return ret;
}

void V8::FreeCString(JSValue &str) {
    if (JS_VALUE_GET_TAG(str) == JS_TAG_CSTRING) {
        CString* cstr = (CString*)JS_VALUE_GET_PTR(str);
        std::free(cstr);
        str.u.ptr = nullptr;
    }
}

Maybe<uint32_t> Value::Uint32Value(Local<Context> context) const {
    int tag = JS_VALUE_GET_TAG(value_);
    if (tag == JS_TAG_INT) {
        return Maybe<uint32_t>((uint32_t)JS_VALUE_GET_INT(value_));
    }
    else if (tag == JS_TAG_FLOAT64){
        return Maybe<uint32_t>((uint32_t)JS_VALUE_GET_FLOAT64(value_));
    }
    else {
        return Maybe<uint32_t>();
    }
}
    
Maybe<int32_t> Value::Int32Value(Local<Context> context) const {
    int tag = JS_VALUE_GET_TAG(value_);
    if (tag == JS_TAG_INT) {
        return Maybe<int32_t>((int32_t)JS_VALUE_GET_INT(value_));
    }
    else if (tag == JS_TAG_FLOAT64){
        return Maybe<int32_t>((int32_t)JS_VALUE_GET_FLOAT64(value_));
    }
    else {
        return Maybe<int32_t>();
    }
}
    
bool Value::IsUndefined() const {
    return JS_IsUndefined(value_);
}

bool Value::IsNull() const {
    return JS_IsNull(value_);
}

bool Value::IsNullOrUndefined() const {
    return JS_IsUndefined(value_) || JS_IsNull(value_);
}

bool Value::IsString() const {
    return JS_VALUE_GET_TAG(value_) == JS_TAG_CSTRING || JS_IsString(value_);
}

bool Value::IsSymbol() const {
    return JS_IsSymbol(value_);
}

void V8FinalizerWrap(JSRuntime *rt, JSValue val) {
    ObjectUserData* objectUdata = reinterpret_cast<ObjectUserData*>(JS_GetOpaque3(val));
    if (objectUdata) {
        if (objectUdata->callback_) {
            objectUdata->callback_(objectUdata);
        }
        js_free_rt(rt, objectUdata);
        JS_SetOpaque(val, nullptr);
    }
}

Isolate::Isolate() : current_context_(nullptr) {
    runtime_ = JS_NewRuntime();
    
    JSClassDef cls_def;
    cls_def.class_name = "__v8_simulate_obj";
    cls_def.finalizer = V8FinalizerWrap;
    cls_def.exotic = NULL;
    cls_def.gc_mark = NULL;
    cls_def.call = NULL;

    //大坑，JSClassID是uint32_t，但Object里的class_id类型为uint16_t，JS_NewClass会把class定义放到以uint32_t索引的数组成员
    //后续如果用这个class_id新建对象，如果class_id大于uint16_t将会被截值，后续释放对象时，会找错class，可能会导致严重后果（不释放，或者调用错误的free）
    class_id_ = 0;
    JS_NewClassID(&class_id_);
    JS_NewClass(runtime_, class_id_, &cls_def);
};

Isolate::~Isolate() {
    JS_FreeRuntime(runtime_);
};

Isolate* Isolate::current_ = nullptr;

void Isolate::handleException() {
    if (currentTryCatch_) {
        currentTryCatch_->handleException();
        return;
    }
    
    JSValue ex = JS_GetException(current_context_->context_);
    
    if (!JS_IsUndefined(ex) && !JS_IsNull(ex)) {
        JSValue fileNameVal = JS_GetProperty(current_context_->context_, ex, JS_ATOM_fileName);
        JSValue lineNumVal = JS_GetProperty(current_context_->context_, ex, JS_ATOM_lineNumber);
        
        auto msg = JS_ToCString(current_context_->context_, ex);
        auto fileName = JS_ToCString(current_context_->context_, fileNameVal);
        auto lineNum = JS_ToCString(current_context_->context_, lineNumVal);
        if (JS_IsUndefined(fileNameVal)) {
            std::cerr << "Uncaught " << msg << std::endl;
        }
        else {
            std::cerr << fileName << ":" << lineNum << ": Uncaught " << msg << std::endl;
        }
        
        JS_FreeCString(current_context_->context_, lineNum);
        JS_FreeCString(current_context_->context_, fileName);
        JS_FreeCString(current_context_->context_, msg);
        
        JS_FreeValue(current_context_->context_, lineNumVal);
        JS_FreeValue(current_context_->context_, fileNameVal);
        
        JS_FreeValue(current_context_->context_, ex);
    }
}

void Isolate::LowMemoryNotification() {
    Scope isolate_scope(this);
    JS_RunGC(runtime_);
}

Context::Context(Isolate* isolate) :isolate_(isolate) {
    context_ = JS_NewContext(isolate->runtime_);
    JS_SetContextOpaque(context_, isolate);
}

bool Value::IsFunction() const {
    return JS_IsFunction(Isolate::current_->GetCurrentContext()->context_, value_);
}

bool Value::IsObject() const {
    return JS_IsObject(value_);
}

bool Value::IsBigInt() const {
    return JS_VALUE_GET_TAG(value_) == JS_TAG_BIG_INT;
}

bool Value::IsBoolean() const {
    return JS_IsBool(value_);
}

bool Value::IsNumber() const {
    return JS_IsNumber(value_);
}

bool Value::IsExternal() const {
    return JS_VALUE_GET_TAG(value_) == JS_TAG_EXTERNAL;
}

MaybeLocal<String> Value::ToString(Local<Context> context) const {
    String * str = new String();
    if (JS_VALUE_GET_TAG(value_) == JS_TAG_CSTRING) {
        str->value_ = value_;
    } else {
        str->value_ = JS_ToString(context->context_, value_);
    }
    return MaybeLocal<String>(Local<String>(str));
}

MaybeLocal<String> String::NewFromUtf8(
    Isolate* isolate, const char* data,
    NewStringType type, int length) {
    auto str = new String();
    //printf("NewFromUtf8:%p\n", str);
    size_t len = length > 0 ? length : strlen(data);
    str->value_ = V8::NewCString(data, len);
    return Local<String>(str);
}

Local<String> String::Empty(Isolate* isolate) {
    static Local<String> _s;
    if (_s.IsEmpty()) {
        _s = Local<String>(new String());
        _s->value_ = V8::NewCString("", 0);
    }
    return _s;
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
    const char *filename = resource_name_.IsEmpty() ? "eval" : *String::Utf8Value(isolate, resource_name_.ToLocalChecked());
    auto ret = JS_Eval(context->context_, *source, source.length(), filename, JS_EVAL_FLAG_STRICT | JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(ret)) {
        isolate->handleException();
        return MaybeLocal<Value>();
    } else {
        Value* val = new Value();
        val->value_ = ret;
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
    JS_INITPTR(external->value_, JS_TAG_EXTERNAL, value);
    return Local<External>(external);
}

void* External::Value() const {
    return JS_VALUE_GET_PTR(value_);
}

double Number::Value() const {
    return JS_VALUE_GET_FLOAT64(value_);
}

Local<Number> Number::New(Isolate* isolate, double value) {
    Number* ret = new Number();
    ret->value_ = JS_NewFloat64_(isolate->GetCurrentContext()->context_, value);
    return Local<Number>(ret);
}

Local<Integer> Integer::New(Isolate* isolate, int32_t value) {
    Integer* ret = new Integer();
    JS_INITVAL(ret->value_, JS_TAG_INT, value);
    return Local<Integer>(ret);
}

bool Boolean::Value() const {
    return JS_VALUE_GET_BOOL(value_);
}

Local<Boolean> Boolean::New(Isolate* isolate, bool value) {
    Boolean* ret = new Boolean();
    JS_INITVAL(ret->value_, JS_TAG_BOOL, (value != 0));
    return Local<Boolean>(ret);
}

Local<Integer> Integer::NewFromUnsigned(Isolate* isolate, uint32_t value) {
    Integer* ret = new Integer();
    ret->value_ = JS_NewUint32_(isolate->GetCurrentContext()->context_, value);;
    return Local<Integer>(ret);
}

int64_t Integer::Value() const {
    if (JS_VALUE_GET_TAG(value_) == JS_TAG_INT) {
        return JS_VALUE_GET_INT(value_);
    } else if (JS_VALUE_GET_TAG(value_) == JS_TAG_FLOAT64) {
        return (int64_t)JS_VALUE_GET_FLOAT64(value_);
    } else {
        return 0;
    }
}

String::Utf8Value::Utf8Value(Isolate* isolate, Local<v8::Value> obj) {
    auto context = isolate->GetCurrentContext();
    if (JS_VALUE_GET_TAG(obj->value_) != JS_TAG_CSTRING) {
        data_ = JS_ToCStringLen(context->context_, &len_, obj->value_);
        context_ = context->context_;
    } else {
        CString* cstr = (CString*)JS_VALUE_GET_PTR(obj->value_);
        data_ = cstr->data;
        len_ = cstr->len;
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
    if (global_.IsEmpty()) {
        Object* obj = new Object();
        obj->value_ = JS_GetGlobalObject(context_);
        global_ = Local<Object>(obj);
    }
    return global_;
}

Context::~Context() {
    JS_FreeValue(context_, global_->value_);
    JS_FreeContext(context_);
}

void Template::Set(Isolate* isolate, const char* name, Local<Data> value) {
    fields_[name] = value;
}

    
void Template::SetAccessorProperty(Local<Name> name,
                                         Local<FunctionTemplate> getter,
                                         Local<FunctionTemplate> setter,
                                         PropertyAttribute attribute) {
    
    accessor_property_infos_[name] = {getter, setter, attribute};
}

void Template::InitPropertys(Local<Context> context, JSValue obj) {
    for(auto it : fields_) {
        JSAtom atom = JS_NewAtom(context->context_, it.first.data());
        Local<FunctionTemplate> funcTpl = Local<FunctionTemplate>::Cast(it.second);
        Local<Function> lfunc = funcTpl->GetFunction(context).ToLocalChecked();
        JS_DefinePropertyValue(context->context_, obj, atom, lfunc->value_, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    }
    
    for (auto it : accessor_property_infos_) {
        JSValue getter = JS_Undefined();
        JSValue setter = JS_Undefined();
        int flag = JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE;
        String::Utf8Value name(context->GetIsolate(), it.first);
        if (!it.second.getter_.IsEmpty()) {
            flag |= JS_PROP_HAS_GET;
            JSCFunctionType pfunc;
            pfunc.getter_magic = [](JSContext *ctx, JSValueConst this_val, int magic) {
                Isolate* isolate = reinterpret_cast<Isolate*>(JS_GetContextOpaque(ctx));
                Local<FunctionTemplate> functionTemplate = isolate->GetFunctionTemplate(magic);
                FunctionCallbackInfo<Value> callbackInfo;
                callbackInfo.isolate_ = isolate;
                callbackInfo.argc_ = 0;
                callbackInfo.argv_ = nullptr;
                callbackInfo.context_ = ctx;
                callbackInfo.this_ = this_val;
                callbackInfo.magic_ = magic;
                callbackInfo.value_ = JS_Undefined();
                callbackInfo.isConstructCall = false;
                
                functionTemplate->callback_(callbackInfo);
                
                return callbackInfo.value_;
            };
            getter = JS_NewCFunction2(context->context_, pfunc.generic, "get", 0, JS_CFUNC_getter_magic, it.second.getter_->magic_);//TODO:名字改为get xxx
        }
        if (!it.second.setter_.IsEmpty()) {
            JSCFunctionType pfunc;
            flag |= JS_PROP_HAS_SET;
            flag |= JS_PROP_WRITABLE;
            pfunc.setter_magic = [](JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic) {
                Isolate* isolate = reinterpret_cast<Isolate*>(JS_GetContextOpaque(ctx));
                Local<FunctionTemplate> functionTemplate = isolate->GetFunctionTemplate(magic);
                FunctionCallbackInfo<Value> callbackInfo;
                callbackInfo.isolate_ = isolate;
                callbackInfo.argc_ = 1;
                callbackInfo.argv_ = &val;
                callbackInfo.context_ = ctx;
                callbackInfo.this_ = this_val;
                callbackInfo.magic_ = magic;
                callbackInfo.value_ = JS_Undefined();
                callbackInfo.isConstructCall = false;
                
                functionTemplate->callback_(callbackInfo);
                
                return callbackInfo.value_;
            };
            setter = JS_NewCFunction2(context->context_, pfunc.generic, "set", 0, JS_CFUNC_setter_magic, it.second.setter_->magic_);//TODO:名字改为set xxx
        }
        JSAtom atom = JS_NewAtom(context->context_, *name);
        JS_DefineProperty(context->context_, obj, atom, JS_Undefined(), getter, setter, flag);
        JS_FreeValue(context->context_, getter);
        JS_FreeValue(context->context_, setter);
    }
}

void ObjectTemplate::SetInternalFieldCount(int value) {
    internal_field_count_ = value;
}


Local<FunctionTemplate> FunctionTemplate::New(Isolate* isolate, FunctionCallback callback,
                                              Local<Value> data) {
    Local<FunctionTemplate> functionTemplate(new FunctionTemplate());
    functionTemplate->callback_ = callback;
    functionTemplate->data_ = data;
    functionTemplate->magic_ = isolate->RegFunctionTemplate(functionTemplate);
    return functionTemplate;
}

Local<ObjectTemplate> FunctionTemplate::InstanceTemplate() {
    if (instance_template_.IsEmpty()) {
        instance_template_ = Local<ObjectTemplate>(new ObjectTemplate());
    }
    return instance_template_;
}
    
void FunctionTemplate::Inherit(Local<FunctionTemplate> parent) {
    //TODO:
}
    
Local<ObjectTemplate> FunctionTemplate::PrototypeTemplate() {
    if (prototype_template_.IsEmpty()) {
        prototype_template_ = Local<ObjectTemplate>(new ObjectTemplate());
    }
    return prototype_template_;
}

MaybeLocal<Function> FunctionTemplate::GetFunction(Local<Context> context) {
    //TODO: cache function for context
    bool isCtor = !prototype_template_.IsEmpty();
    JSValue func = JS_NewCFunctionMagic(context->context_, [](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
        Isolate* isolate = reinterpret_cast<Isolate*>(JS_GetContextOpaque(ctx));
        Local<FunctionTemplate> functionTemplate = isolate->GetFunctionTemplate(magic);
        FunctionCallbackInfo<Value> callbackInfo;
        callbackInfo.isolate_ = isolate;
        callbackInfo.argc_ = argc;
        callbackInfo.argv_ = argv;
        callbackInfo.context_ = ctx;
        callbackInfo.this_ = this_val;
        callbackInfo.magic_ = magic;
        callbackInfo.value_ = JS_Undefined();
        callbackInfo.isConstructCall = !functionTemplate->instance_template_.IsEmpty();
        
        if (callbackInfo.isConstructCall && functionTemplate->instance_template_->internal_field_count_ > 0) {
            JSValue proto = JS_GetProperty(ctx, this_val, JS_ATOM_prototype);
            callbackInfo.this_ = JS_NewObjectProtoClass(ctx, proto, isolate->class_id_);
            JS_FreeValue(ctx, proto);
            size_t size = sizeof(ObjectUserData) + sizeof(void*) * (functionTemplate->InstanceTemplate()->internal_field_count_ - 1);
            ObjectUserData* object_udata = (ObjectUserData*)js_malloc(ctx, size);
            memset(object_udata, 0, size);
            object_udata->len_ = functionTemplate->instance_template_->internal_field_count_;
            JS_SetOpaque(callbackInfo.this_, object_udata);
        }
        
        functionTemplate->callback_(callbackInfo);
        
        return callbackInfo.isConstructCall ? callbackInfo.this_ : callbackInfo.value_;
    }, "native", 0, isCtor ? JS_CFUNC_constructor_magic : JS_CFUNC_generic_magic, magic_); //TODO: native可以替换成成员函数名
    
    if (isCtor) {
        JSValue proto = JS_NewObject(context->context_);
        prototype_template_->InitPropertys(context, proto);
        InitPropertys(context, func);
        JS_SetConstructor(context->context_, func, proto);
        JS_FreeValue(context->context_, proto);
    }
    
    Function* function = new Function();
    function->value_ = func;
    
    return MaybeLocal<Function>(Local<Function>(function));
}

Maybe<bool> Object::Set(Local<Context> context,
                        Local<Value> key, Local<Value> value) {
    JSValue jsValue;
    if (JS_VALUE_GET_TAG(value->value_) != JS_TAG_CSTRING) {
        jsValue = value->value_;
    } else {
        CString* cstr = (CString*)JS_VALUE_GET_PTR(value->value_);
        jsValue = JS_NewStringLen(context->context_, cstr->data, cstr->len);
    }
    if (JS_VALUE_GET_TAG(key->value_) == JS_TAG_CSTRING) {
        CString* cstr = (CString*)JS_VALUE_GET_PTR(key->value_);
        JS_SetPropertyStr(context->context_, value_, cstr->data, jsValue);
    } else {
        if (key->IsNumber()) {
            JS_SetPropertyUint32(context->context_, value_, key->Uint32Value(context).ToChecked(), jsValue);
        } else {
            JS_SetProperty(context->context_, value_, JS_ValueToAtom(context->context_, key->value_), jsValue);
        }
    }
    
    return Maybe<bool>(true);
}

void Object::SetAlignedPointerInInternalField(int index, void* value) {
    ObjectUserData* objectUdata = reinterpret_cast<ObjectUserData*>(JS_GetOpaque3(value_));
    if (!objectUdata || index >= objectUdata->len_) {
        std::cerr << "SetAlignedPointerInInternalField";
        if (objectUdata) {
            std::cerr << ", index out of range, index = " << index << ", length=" << objectUdata->len_ << std::endl;
        }
        else {
            std::cerr << "internalFields is nullptr " << std::endl;
        }
            
        abort();
    }
    objectUdata->ptrs_[index] = value;
}
    
void* Object::GetAlignedPointerFromInternalField(int index) {
    ObjectUserData* objectUdata = reinterpret_cast<ObjectUserData*>(JS_GetOpaque3(value_));
    
    bool noObjectUdata = IsFunction() || objectUdata == nullptr;

    if (noObjectUdata || index >= objectUdata->len_) {
        std::cerr << "GetAlignedPointerFromInternalField";
        if (!noObjectUdata) {
            std::cerr << ", index out of range, index = " << index << ", length=" << objectUdata->len_ << std::endl;
        }
        else {
            std::cerr << ", internalFields is nullptr " << std::endl;
        }
            
        abort();
    }
    return objectUdata->ptrs_[index];
}

TryCatch::TryCatch(Isolate* isolate) {
    isolate_ = isolate;
    catched_ = JS_Undefined();
    prev_ = isolate_->currentTryCatch_;
    isolate_->currentTryCatch_ = this;
}
    
TryCatch::~TryCatch() {
    isolate_->currentTryCatch_ = prev_;
    JS_FreeValue(isolate_->current_context_->context_, catched_);
}
    
bool TryCatch::HasCaught() const {
    return !JS_IsUndefined(catched_) && !JS_IsNull(catched_);
}
    
Local<Value> TryCatch::Exception() const {
    Local<Value> ret = Local<Value>(new Value());
    ret->value_ = catched_;
    return ret;
}

MaybeLocal<Value> TryCatch::StackTrace(Local<Context> context) {
    if (stacktrace_.length() == 0) {
        JSValue stackVal = JS_GetProperty(isolate_->current_context_->context_, catched_, JS_ATOM_stack);
        const char* stack = JS_ToCString(isolate_->current_context_->context_, stackVal);
        stacktrace_ = stack;
        JS_FreeCString(isolate_->current_context_->context_, stack);
    }
    auto str = new String();
    str->value_ = V8::NewCString(stacktrace_.data(), stacktrace_.length());
    return MaybeLocal<Value>(Local<String>(str));
}
    
Local<v8::Message> TryCatch::Message() const {
    JSValue fileNameVal = JS_GetProperty(isolate_->current_context_->context_, catched_, JS_ATOM_fileName);
    JSValue lineNumVal = JS_GetProperty(isolate_->current_context_->context_, catched_, JS_ATOM_lineNumber);
    
    Local<v8::Message> message(new v8::Message());
    
    if (JS_IsUndefined(fileNameVal)) {
        message->resource_name_ = "<unknow>";
        message->line_number_ = - 1;
    } else {
        const char* fileName = JS_ToCString(isolate_->current_context_->context_, fileNameVal);
        message->resource_name_ = fileName;
        JS_FreeCString(isolate_->current_context_->context_, fileName);
        JS_ToInt32(isolate_->current_context_->context_, &message->line_number_, lineNumVal);
    }
    
    JS_FreeValue(isolate_->current_context_->context_, lineNumVal);
    JS_FreeValue(isolate_->current_context_->context_, fileNameVal);
    
    return message;
}

void TryCatch::handleException() {
    catched_ = JS_GetException(isolate_->current_context_->context_);
}

}  // namespace v8
