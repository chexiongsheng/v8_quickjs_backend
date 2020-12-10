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
    return JS_IsString(value_);
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
    literal_values_[kUndefinedValueIndex] = JS_Undefined();
    literal_values_[kNullValueIndex] = JS_Null();
    literal_values_[kTrueValueIndex] = JS_True();
    literal_values_[kFalseValueIndex] = JS_False();
    literal_values_[kEmptyStringIndex] = JS_Undefined();
    
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
    for (int i = 0; i < values_.size(); i++) {
        delete values_[i];
    }
    values_.clear();
    JS_FreeValueRT(runtime_, literal_values_[kEmptyStringIndex]);
    JS_FreeRuntime(runtime_);
};

Value* Isolate::Alloc_() {
    if (value_alloc_pos_ == values_.size()) {
        JSValue* node = new JSValue;
        *node = JS_Undefined();
        values_.push_back(node);
    }
    auto ret = reinterpret_cast<Value*>(values_[value_alloc_pos_++]);
    return ret;
}

void Isolate::ForeachAllocValue(int start, int end, std::function<void(JSValue*, int)> callback) {
    for (int i = std::min(end, value_alloc_pos_) ; i > std::max(0, start); i--) {
        int idx = i - 1;
        JSValue * to_free = values_[idx];
        callback(to_free, idx);
    }
}

void Isolate::Escape(JSValue* val) {
    V8::Check(currentHandleScope, "try to escape a scope, but no scope register!");
    currentHandleScope->Escape(val);
}

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

void HandleScope::Escape(JSValue* val) {
    escapes_.insert(val);
}

void HandleScope::Exit() {
    if(prev_pos_ < isolate_->value_alloc_pos_) {
        //std::cout << "---------------- start HandleScope::Exit -------------------" << std::endl;
        //std::cout << prev_pos_ << "," << isolate_->value_alloc_pos_ << std::endl;
        isolate_->ForeachAllocValue(prev_pos_, isolate_->value_alloc_pos_, [this](JSValue* val, int idx){
            if (JS_VALUE_HAS_REF_COUNT(*val)) {
                if (this->escapes_.find(val) == this->escapes_.end()) { //not excaped
                    //std::cout << "free val type:" << JS_VALUE_GET_TAG(*val) << "," << val << ", idx:" << idx << std::endl;
                    JS_FreeValueRT(isolate_->runtime_, *val);
                //} else {
                    //std::cout << "escaped val type:" << JS_VALUE_GET_TAG(*val) << "," << val << std::endl;
                }
            }
        });
        isolate_->value_alloc_pos_ = prev_pos_;
        //std::cout << "---------------- end HandleScope::Exit -------------------" << std::endl;
    }
}

bool Value::IsFunction() const {
    return JS_IsFunction(Isolate::current_->GetCurrentContext()->context_, value_);
}

bool Value::IsDate() const {
    return JS_IsDate(value_);
}

bool Value::IsArrayBuffer() const {
    return JS_IsArrayBuffer(value_);
}

bool Value::IsArrayBufferView() const {
    return JS_IsArrayBufferView(value_);
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
    if (JS_IsString(value_)) {
        return MaybeLocal<String>(Local<String>(static_cast<String*>(const_cast<Value*>(this))));
    } else {
        //由HandleScope跟踪回收
        String * str = context->GetIsolate()->Alloc<String>();
        str->value_ = JS_ToString(context->context_, value_);
        return MaybeLocal<String>(Local<String>(str));
    }
    
}

MaybeLocal<String> String::NewFromUtf8(
    Isolate* isolate, const char* data,
    NewStringType type, int length) {
    String *str = isolate->Alloc<String>();
    //printf("NewFromUtf8:%p\n", str);
    size_t len = length > 0 ? length : strlen(data);
    str->value_ = JS_NewStringLen(isolate->current_context_->context_, data, len);
    return Local<String>(str);
}

Local<String> String::Empty(Isolate* isolate) {
    if (JS_IsUndefined(isolate->literal_values_[kEmptyStringIndex])) {
        isolate->literal_values_[kEmptyStringIndex] = JS_NewStringLen(isolate->current_context_->context_, "", 0);
    }
    return Local<String>(reinterpret_cast<String*>(&isolate->literal_values_[kEmptyStringIndex]));
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

static V8_INLINE MaybeLocal<Value> ProcessResult(Isolate *isolate, JSValue ret) {
    if (JS_IsException(ret)) {
        isolate->handleException();
        return MaybeLocal<Value>();
    } else {
        //脚本执行的返回值由HandleScope接管，这可能有需要GC的对象
        Value* val = isolate->Alloc<Value>();
        val->value_ = ret;
        return MaybeLocal<Value>(Local<Value>(val));
    }
}

MaybeLocal<Value> Script::Run(Local<Context> context) {
    auto isolate = context->GetIsolate();

    String::Utf8Value source(isolate, source_);
    const char *filename = resource_name_.IsEmpty() ? "eval" : *String::Utf8Value(isolate, resource_name_.ToLocalChecked());
    auto ret = JS_Eval(context->context_, *source, source.length(), filename, JS_EVAL_FLAG_STRICT | JS_EVAL_TYPE_GLOBAL);

    return ProcessResult(isolate, ret);
}

Script::~Script() {
    //JS_FreeValue(context_->context_, source_->value_);
    //if (!resource_name_.IsEmpty()) {
    //    JS_FreeValue(context_->context_, resource_name_.ToLocalChecked()->value_);
    //}
}

Local<External> External::New(Isolate* isolate, void* value) {
    External* external = isolate->Alloc<External>();
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
    Number* ret = isolate->Alloc<Number>();
    ret->value_ = JS_NewFloat64_(isolate->GetCurrentContext()->context_, value);
    return Local<Number>(ret);
}

Local<Integer> Integer::New(Isolate* isolate, int32_t value) {
    Integer* ret = isolate->Alloc<Integer>();
    JS_INITVAL(ret->value_, JS_TAG_INT, value);
    return Local<Integer>(ret);
}

Local<Integer> Integer::NewFromUnsigned(Isolate* isolate, uint32_t value) {
    Integer* ret = isolate->Alloc<Integer>();
    ret->value_ = JS_NewUint32_(isolate->GetCurrentContext()->context_, value);;
    return Local<Integer>(ret);
}

Local<BigInt> BigInt::New(Isolate* isolate, int64_t value) {
    BigInt* ret = isolate->Alloc<BigInt>();
    ret->value_ = JS_NewBigInt64(isolate->current_context_->context_, value);
    return Local<BigInt>(ret);
}

Local<BigInt> BigInt::NewFromUnsigned(Isolate* isolate, uint64_t value) {
    BigInt* ret = isolate->Alloc<BigInt>();
    ret->value_ = JS_NewBigUint64(isolate->current_context_->context_, value);
    return Local<BigInt>(ret);
}

uint64_t BigInt::Uint64Value(bool* lossless) const {
    return static_cast<uint64_t>(Int64Value(lossless));
}

int64_t BigInt::Int64Value(bool* lossless) const {
    int64_t ret;
    JS_ToBigInt64(Isolate::current_->current_context_->context_, &ret, value_);
    return ret;
}

bool Boolean::Value() const {
    return JS_VALUE_GET_BOOL(value_);
}

Local<Boolean> Boolean::New(Isolate* isolate, bool value) {
    Boolean* ret = isolate->Alloc<Boolean>();
    JS_INITVAL(ret->value_, JS_TAG_BOOL, (value != 0));
    return Local<Boolean>(ret);
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
    data_ = JS_ToCStringLen(context->context_, &len_, obj->value_);
    context_ = context->context_;
}

String::Utf8Value::~Utf8Value() {
    if (context_) {
        JS_FreeCString(context_, data_);
    }
}

MaybeLocal<Value> Date::New(Local<Context> context, double time) {
    Date *date = context->GetIsolate()->Alloc<Date>();
    date->value_ = JS_NewDate(context->context_, time);
    return MaybeLocal<Value>(Local<Date>(date));
}
    
double Date::ValueOf() const {
    return JS_GetDate(Isolate::current_->current_context_->context_, value_);
}

static std::vector<uint8_t> dummybuffer;

Local<ArrayBuffer> ArrayBuffer::New(Isolate* isolate, size_t byte_length) {
    ArrayBuffer *ab = isolate->Alloc<ArrayBuffer>();
    if (dummybuffer.size() < byte_length) dummybuffer.resize(byte_length, 0);
    ab->value_ = JS_NewArrayBufferCopy(isolate->current_context_->context_, dummybuffer.data(), byte_length);
    return Local<ArrayBuffer>(ab);
}

Local<ArrayBuffer> ArrayBuffer::New(Isolate* isolate, void* data, size_t byte_length,
                                           ArrayBufferCreationMode mode) {
    V8::Check(mode == ArrayBufferCreationMode::kExternalized, "only ArrayBufferCreationMode::kExternalized support!");
    ArrayBuffer *ab = isolate->Alloc<ArrayBuffer>();
    ab->value_ = JS_NewArrayBuffer(isolate->current_context_->context_, (uint8_t*)data, byte_length, nullptr, nullptr, false);
    return Local<ArrayBuffer>(ab);
}

ArrayBuffer::Contents ArrayBuffer::GetContents() {
    ArrayBuffer::Contents ret;
    ret.data_ = JS_GetArrayBuffer(Isolate::current_->current_context_->context_, &ret.byte_length_, value_);
    return ret;
}

Local<ArrayBuffer> ArrayBufferView::Buffer() {
    Isolate* isolate = Isolate::current_;
    ArrayBuffer* ab = isolate->Alloc<ArrayBuffer>();
    ab->value_ = JS_GetArrayBufferView(isolate->current_context_->context_, value_);
    return Local<ArrayBuffer>(ab);
}
    
size_t ArrayBufferView::ByteOffset() {
    size_t byte_offset;
    size_t byte_length;
    size_t bytes_per_element;
    JS_GetArrayBufferViewInfo(Isolate::current_->current_context_->context_, value_, &byte_offset, &byte_length, &bytes_per_element);
    return byte_offset;
}
    
size_t ArrayBufferView::ByteLength() {
    size_t byte_offset;
    size_t byte_length;
    size_t bytes_per_element;
    JS_GetArrayBufferViewInfo(Isolate::current_->current_context_->context_, value_, &byte_offset, &byte_length, &bytes_per_element);
    return byte_length;
}

int Isolate::RegFunctionTemplate(Local<FunctionTemplate> data) {
    function_templates_.push_back(data);
    return function_templates_.size() - 1;
}

Local<FunctionTemplate>& Isolate::GetFunctionTemplate(int index) {
    return function_templates_[index];
}

Local<Object> Context::Global() {
    Object *g = reinterpret_cast<Object*>(&global_);
    return Local<Object>(g);
}


Context::Context(Isolate* isolate) :isolate_(isolate) {
    context_ = JS_NewContext(isolate->runtime_);
    JS_SetContextOpaque(context_, this);
    global_ = JS_GetGlobalObject(context_);
}

Context::~Context() {
    JS_FreeValue(context_, global_);
    JS_FreeContext(context_);
}

MaybeLocal<Value> Function::Call(Local<Context> context,
                             Local<Value> recv, int argc,
                             Local<Value> argv[]) {
    Isolate *isolate = context->GetIsolate();
    JSValue *js_this = reinterpret_cast<JSValue*>(recv.IsEmpty() ? isolate->Undefined() : (*recv));
    JSValue *js_argv = (JSValue*)alloca(argc * sizeof(JSValue));
    for(int i = 0 ; i < argc; i++) {
        isolate->Escape(*argv[i]);
        js_argv[i] = argv[i]->value_;
    }
    JSValue ret = JS_Call(context->context_, value_, *js_this, argc, js_argv);
    
    return ProcessResult(isolate, ret);
}

void Template::Set(Isolate* isolate, const char* name, Local<Data> value) {
    fields_[name] = value;
}

    
void Template::SetAccessorProperty(Local<Name> name,
                                         Local<FunctionTemplate> getter,
                                         Local<FunctionTemplate> setter,
                                         PropertyAttribute attribute) {
    
    accessor_property_infos_[*String::Utf8Value(Isolate::current_, name)] = {getter, setter, attribute};
}

void Template::InitPropertys(Local<Context> context, JSValue obj) {
    for(auto it : fields_) {
        JSAtom atom = JS_NewAtom(context->context_, it.first.data());
        Local<FunctionTemplate> funcTpl = Local<FunctionTemplate>::Cast(it.second);
        Local<Function> lfunc = funcTpl->GetFunction(context).ToLocalChecked();
        context->GetIsolate()->Escape(*lfunc);
        JS_DefinePropertyValue(context->context_, obj, atom, lfunc->value_, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    }
    
    for (auto it : accessor_property_infos_) {
        JSValue getter = JS_Undefined();
        JSValue setter = JS_Undefined();
        int flag = JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE;
        std::string name = it.first;
        if (!it.second.getter_.IsEmpty()) {
            flag |= JS_PROP_HAS_GET;
            JSCFunctionType pfunc;
            pfunc.getter_magic = [](JSContext *ctx, JSValueConst this_val, int magic) {
                Isolate* isolate = reinterpret_cast<Context*>(JS_GetContextOpaque(ctx))->GetIsolate();
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
            std::string getter_name = "get " + name;
            getter = JS_NewCFunction2(context->context_, pfunc.generic, getter_name.c_str(), 0, JS_CFUNC_getter_magic, it.second.getter_->magic_);
        }
        if (!it.second.setter_.IsEmpty()) {
            JSCFunctionType pfunc;
            flag |= JS_PROP_HAS_SET;
            flag |= JS_PROP_WRITABLE;
            pfunc.setter_magic = [](JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic) {
                Isolate* isolate = reinterpret_cast<Context*>(JS_GetContextOpaque(ctx))->GetIsolate();
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
            std::string setter_name = "set " + name;
            setter = JS_NewCFunction2(context->context_, pfunc.generic, setter_name.c_str(), 0, JS_CFUNC_setter_magic, it.second.setter_->magic_);
        }
        JSAtom atom = JS_NewAtom(context->context_, name.c_str());
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
    if (data.IsEmpty()) {
        functionTemplate->data_ = JS_Undefined();
    } else {
        functionTemplate->data_ = data->value_;
    }
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
    parent_ = parent;
}
    
Local<ObjectTemplate> FunctionTemplate::PrototypeTemplate() {
    if (prototype_template_.IsEmpty()) {
        prototype_template_ = Local<ObjectTemplate>(new ObjectTemplate());
    }
    return prototype_template_;
}

MaybeLocal<Function> FunctionTemplate::GetFunction(Local<Context> context) {
    auto iter = context_to_funtion_.find(*context);
    if (iter != context_to_funtion_.end()) {
        return iter->second.Get(context->GetIsolate());
    }
    bool isCtor = !prototype_template_.IsEmpty();
    JSValue func = JS_NewCFunctionMagic(context->context_, [](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
        Isolate* isolate = reinterpret_cast<Context*>(JS_GetContextOpaque(ctx))->GetIsolate();
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
        
        if (!parent_.IsEmpty()) {
            Local<Function> parent_func = parent_->GetFunction(context).ToLocalChecked();
            JSValue parent_proto = JS_GetProperty(context->context_, parent_func->value_, JS_ATOM_prototype);
            JS_SetPrototype(context->context_, proto, parent_proto);
            JS_FreeValue(context->context_, parent_proto);
        }
    }
    
    Function* function = context->GetIsolate()->Alloc<Function>();
    function->value_ = func;
    
    Local<Function> ret(function);
    
    context_to_funtion_[*context] = Global<Function>(context->GetIsolate(), ret);
    
    return MaybeLocal<Function>(ret);
}

Maybe<bool> Object::Set(Local<Context> context,
                        Local<Value> key, Local<Value> value) {
    if (key->IsNumber()) {
        JS_SetPropertyUint32(context->context_, value_, key->Uint32Value(context).ToChecked(), value->value_);
    } else {
        JS_SetProperty(context->context_, value_, JS_ValueToAtom(context->context_, key->value_), value->value_);
    }
    
    context->GetIsolate()->Escape(*value);
    
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
    return Local<Value>(reinterpret_cast<Value*>(const_cast<JSValue*>(&catched_)));
}

MaybeLocal<Value> TryCatch::StackTrace(Local<Context> context) {
    auto str = context->GetIsolate()->Alloc<String>();
    str->value_ = JS_GetProperty(isolate_->current_context_->context_, catched_, JS_ATOM_stack);;
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
