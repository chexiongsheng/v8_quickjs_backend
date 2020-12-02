#ifndef INCLUDE_V8_H_
#define INCLUDE_V8_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <iostream>
#include <map>

#include "libplatform/libplatform.h"

#include "v8config.h"     // NOLINT(build/include_directory)
#include "quickjs-msvc.h"

#define JS_TAG_EXTERNAL (JS_TAG_FLOAT64 + 1)

namespace v8 {
class Object;
class Isolate;
class Context;
template <class T>
class MaybeLocal;
class FunctionTemplate;
template<typename T>
class FunctionCallbackInfo;
class String;

class V8_EXPORT StartupData {
public:
    const char* data;
    int raw_size;
};

class V8_EXPORT V8 {
public:
    V8_INLINE static void SetSnapshotDataBlob(StartupData* startup_blob) {
        //Do nothing
    }

    V8_INLINE static void InitializePlatform(Platform* platform) {
        //Do nothing
    }

    V8_INLINE static bool Initialize() {
        //Do nothing
        return true;
    }

    V8_INLINE static void ShutdownPlatform() {
        //Do nothing
    }

    V8_INLINE static bool Dispose() {
        //Do nothing
        return true;
    }

    V8_INLINE static void FromJustIsNothing() {
        //Utils::ApiCheck(false, "v8::FromJust", "Maybe value is Nothing.");
        std::cerr << "v8::FromJust, Maybe value is Nothing." << std::endl;
        abort();
    }
};

//用智能指针实现
template <class T>
class Local {
public:
    V8_INLINE Local() : val_(nullptr), counter_(nullptr) {}
    template <class S>
    V8_INLINE Local(Local<S> that) {
        static_assert(std::is_base_of<T, S>::value, "type check");
        val_ = reinterpret_cast<T*>(*that);
        counter_ = that.counter_;
        if (val_ != nullptr) {
            ++(*counter_);
        }
    }

    V8_INLINE Local(const Local<T> &that) {
        val_ = reinterpret_cast<T*>(*that);
        counter_ = that.counter_;
        if (val_ != nullptr) {
            ++(*counter_);
        }
    }

    explicit V8_INLINE Local(T* that) {
        val_ = that;
        if (val_ == nullptr) {
            counter_ = nullptr;
        }
        else {
            counter_ = new int(1);
        }
    }

    Local<T>& operator=(const Local<T>& that) {
        if (&that == this) {
            return *this;
        }
        Clear();
        if (that.IsEmpty()) {
            return *this;
        }
        val_ = that.val_;
        counter_ = that.counter_;
        ++(*counter_);
        return *this;
    }

    ~Local() {
        Clear();
    }

    V8_INLINE bool IsEmpty() const { return val_ == nullptr; }

    V8_INLINE void Clear() {
        if (val_ == nullptr) return;
        --(*counter_);
        if (*counter_ == 0) {
            delete val_;
            delete counter_;
        }
        val_ = nullptr;
        counter_ = nullptr;
    }

    V8_INLINE T* operator->() const { return val_; }

    V8_INLINE T* operator*() const { return val_; }

    template <class S> V8_INLINE static Local<T> Cast(Local<S> that) {
#ifdef V8_ENABLE_CHECKS
        // If we're going to perform the type check then we have to check
        // that the handle isn't empty before doing the checked cast.
        if (that.IsEmpty()) return Local<T>();
#endif
        Local<T> result;
        result.val_ = T::Cast(*that);
        if (result.val_ != nullptr) {
            result.counter_ = that.counter_;
            ++(*(result.counter_));
        }
        return result;
    }

    template <class S>
    V8_INLINE Local<S> As() const {
        return Local<S>::Cast(*this);
    }
    
    bool operator==(const Local<T>& that) const {
        return val_ == that.val_;
    }
    
    bool operator<(const Local<T>& that)  const {
        return val_ < that.val_;
    }


    //V8_INLINE static Local<T> New(Isolate* isolate, Local<T> that);
    //V8_INLINE static Local<T> New(Isolate* isolate,
    //                              const PersistentBase<T>& that);
    //V8_INLINE static Local<T> New(Isolate* isolate,
    //                              const TracedReferenceBase<T>& that);


private:
    friend class Object;
    friend class Context;
    friend class String;
    friend class Isolate;
    template <class F>
    friend class MaybeLocal;
    template <class F>
    friend class Local;

    T* val_;
    int* counter_;
};

template <class T>
class Maybe {
public:
    V8_INLINE bool IsNothing() const { return !has_value_; }
    V8_INLINE bool IsJust() const { return has_value_; }

    /**
     * An alias for |FromJust|. Will crash if the Maybe<> is nothing.
     */
    V8_INLINE T ToChecked() const { return FromJust(); }

    /**
     * Short-hand for ToChecked(), which doesn't return a value. To be used, where
     * the actual value of the Maybe is not needed like Object::Set.
     */
    V8_INLINE void Check() const {
        if (V8_UNLIKELY(!IsJust())) V8::FromJustIsNothing();
    }

    /**
     * Converts this Maybe<> to a value of type T. If this Maybe<> is
     * nothing (empty), |false| is returned and |out| is left untouched.
     */
    V8_WARN_UNUSED_RESULT V8_INLINE bool To(T* out) const {
        if (V8_LIKELY(IsJust())) *out = value_;
        return IsJust();
    }

    /**
     * Converts this Maybe<> to a value of type T. If this Maybe<> is
     * nothing (empty), V8 will crash the process.
     */
    V8_INLINE T FromJust() const {
        if (V8_UNLIKELY(!IsJust())) V8::FromJustIsNothing();
        return value_;
    }

    /**
     * Converts this Maybe<> to a value of type T, using a default value if this
     * Maybe<> is nothing (empty).
     */
    V8_INLINE T FromMaybe(const T& default_value) const {
        return has_value_ ? value_ : default_value;
    }

    V8_INLINE bool operator==(const Maybe& other) const {
        return (IsJust() == other.IsJust()) &&
            (!IsJust() || FromJust() == other.FromJust());
    }

    V8_INLINE bool operator!=(const Maybe& other) const {
        return !operator==(other);
    }

    Maybe() : has_value_(false) {}
    explicit Maybe(const T& t) : has_value_(true), value_(t) {}

private:

    bool has_value_;
    T value_;

    template <class U>
    friend Maybe<U> Nothing();
    template <class U>
    friend Maybe<U> Just(const U& u);
};

template <class T>
class MaybeLocal {
public:
    V8_INLINE MaybeLocal() : val_(nullptr) {}
    template <class S>
    V8_INLINE MaybeLocal(Local<S> that)
        : val_(that) {
        static_assert(std::is_base_of<T, S>::value, "type check");
    }

    V8_INLINE bool IsEmpty() const { return val_.IsEmpty(); }

    /**
     * Converts this MaybeLocal<> to a Local<>. If this MaybeLocal<> is empty,
     * |false| is returned and |out| is left untouched.
     */
    template <class S>
    V8_WARN_UNUSED_RESULT V8_INLINE bool ToLocal(Local<S>* out) const {
        *out = this->val_;
        return !IsEmpty();
    }

    /**
     * Converts this MaybeLocal<> to a Local<>. If this MaybeLocal<> is empty,
     * V8 will crash the process.
     */
    V8_INLINE Local<T> ToLocalChecked() {
        return val_;
    }

    /**
        * Converts this MaybeLocal<> to a Local<>, using a default value if this
        * MaybeLocal<> is empty.
        */
    template <class S>
    V8_INLINE Local<S> FromMaybe(Local<S> default_value) const {
        return IsEmpty() ? default_value : Local<S>(val_);
    }

private:
    Local<T> val_;
};


class V8_EXPORT Data {
public:
    virtual ~Data() {}
    // private:
    //  Data();
};

typedef union ValueStore {
    JSValue value_;
    struct {
        char* data_;
        size_t len_;
    } str_;
} ValueStore;

typedef struct InternalFields {
    int32_t len_;
    void* ptrs_[1];
} InternalFields;

class V8_EXPORT Value : public Data {
public:
    V8_WARN_UNUSED_RESULT Maybe<uint32_t> Uint32Value(Local<Context> context) const ;
    
    V8_WARN_UNUSED_RESULT Maybe<int32_t> Int32Value(Local<Context> context) const;
    
    bool IsUndefined() const;
    
    bool IsNull() const;

    bool IsNullOrUndefined() const;

    bool IsString() const;

    bool IsSymbol() const;

    //bool IsFunction() const;
    
    //似乎得对quickjs进行一定的改造S
    //V8_INLINE bool IsArrayBuffer() const;
    
    //V8_INLINE bool IsArrayBufferView() const;
    
    //V8_INLINE bool IsDate() const;

    bool IsObject() const;

    bool IsBigInt() const;

    bool IsBoolean() const;

    bool IsNumber() const;

    bool IsExternal() const;
    
    V8_WARN_UNUSED_RESULT MaybeLocal<String> ToString(
        Local<Context> context) const;

    ValueStore u_;
    
    bool jsValue_ = true;
    
    //virtual ~Value() {
        //if (context_) {
        //    JS_FreeValue(context_, value_);
        //}
    //}
};

class V8_EXPORT Object : public Value {
public:
    V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context,
        Local<Value> key, Local<Value> value);
    
    void SetAlignedPointerInInternalField(int index, void* value);
    
    void* GetAlignedPointerFromInternalField(int index);
};

class V8_EXPORT ArrayBuffer : public Object {
public:
    class V8_EXPORT Allocator { // NOLINT
    public:
        virtual ~Allocator() = default;

        V8_INLINE static Allocator* NewDefaultAllocator() {
            return nullptr;
        }
    };
};

class V8_EXPORT Isolate {
public:
    struct CreateParams {
        CreateParams()
            : array_buffer_allocator(nullptr) {}
        ArrayBuffer::Allocator* array_buffer_allocator;
    };

    class V8_EXPORT Scope {
    public:
        explicit V8_INLINE Scope(Isolate* isolate) /*: isolate_(isolate) */{ }

        V8_INLINE ~Scope() { }

        // Prevent copying of Scope objects.
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        //Isolate* const isolate_;
    };

    V8_INLINE static Isolate* New(const CreateParams& params) {
        return new Isolate();
    }

    V8_INLINE void Dispose() {
        delete this;
    }

    V8_INLINE Local<Context> GetCurrentContext() {
        return current_context_;
    }
    
    int RegFunctionTemplate(Local<FunctionTemplate> data);
    
    Local<FunctionTemplate>& GetFunctionTemplate(int index);

    JSRuntime *runtime_;
    
    JSClassID class_id_;

    Local<Context> current_context_;
    
    std::vector<Local<FunctionTemplate>> function_templates_;

    Isolate();

    ~Isolate();
};

class V8_EXPORT Context {
public:
    V8_INLINE static Local<Context> New(Isolate* isolate) {
        return Local<Context>(new Context(isolate));
    }

    Local<Object> Global();

    V8_INLINE Isolate* GetIsolate() { return isolate_; }

    class Scope {
    public:
        explicit V8_INLINE Scope(Local<Context> context) : context_(context), prev_context_(nullptr) {
            prev_context_ = context->isolate_->current_context_;
            context->isolate_->current_context_ = context_;
        }
        V8_INLINE ~Scope() {
            context_->isolate_->current_context_ = prev_context_;
        }

    private:
        Local<Context> context_;
        Local<Context> prev_context_;
    };

    ~Context();

    Isolate* const isolate_;

    JSContext *context_;
    
    Local<Object> global_;

    Context(Isolate* isolate);
};


class V8_EXPORT External : public Value {
public:
    static Local<External> New(Isolate* isolate, void* value);
    V8_INLINE static External* Cast(Value* obj) {
        return static_cast<External*>(obj);
    }
    void* Value() const;
};

class V8_EXPORT Primitive : public Value { };

class V8_EXPORT Number : public Primitive {
public:
    double Value() const ;
    
    static Local<Number> New(Isolate* isolate, double value);
    
    V8_INLINE static Number* Cast(v8::Value* obj) {
        return static_cast<Number*>(obj);
    }
};

class V8_EXPORT Integer : public Number {
public:
    static Local<Integer> New(Isolate* isolate, int32_t value);
    
    static Local<Integer> NewFromUnsigned(Isolate* isolate, uint32_t value);
    
    int64_t Value() const ;
    
    V8_INLINE static Integer* Cast(v8::Value* obj) {
        return static_cast<Integer*>(obj);
    }
};

class V8_EXPORT Boolean : public Primitive {
public:
    bool Value() const;
    
    V8_INLINE static Boolean* Cast(v8::Value* obj) {
        return static_cast<Boolean*>(obj);
    }
    static Local<Boolean> New(Isolate* isolate, bool value);
};

class V8_EXPORT Name : public Primitive {
public:

    V8_INLINE static Name* Cast(Value* obj) {
        return static_cast<Name*>(obj);
    }

private:
};

enum class NewStringType {
    /**
     * Create a new string, always allocating new storage memory.
     */
    kNormal,

    /**
     * Acts as a hint that the string should be created in the
     * old generation heap space and be deduplicated if an identical string
     * already exists.
     */
    kInternalized
};

class V8_EXPORT String : public Name {
public:
    /**
     * Returns the number of bytes in the UTF-8 encoded
     * representation of this string.
     */
    //int Utf8Length(Isolate* isolate) const {
        //TODO: to be implement
    //    return 0;
    //}

    // UTF-8 encoded characters.
    //int WriteUtf8(Isolate* isolate, char* buffer, int length = -1,
    //            int* nchars_ref = nullptr) const {
    //    size_t size;
    //
    //    return 0;
    //}

    /*
    * A zero length string.
    */
    //V8_INLINE static Local<String> Empty(Isolate* isolate) {
    //    return Local<String>(new String());
    //}

    V8_INLINE static String* Cast(v8::Value* obj) {
        return static_cast<String*>(obj);
    }

    static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromUtf8(
        Isolate* isolate, const char* data, NewStringType type = NewStringType::kNormal, int length = -1);


    class V8_EXPORT Utf8Value {
    public:
        Utf8Value(Isolate* isolate, Local<v8::Value> obj);
        
        ~Utf8Value();
        
        char* operator*() { return data_; }
        const char* operator*() const { return data_; }
        int length() const { return len_; }

        // Disallow copying and assigning.
        Utf8Value(const Utf8Value&) = delete;
        void operator=(const Utf8Value&) = delete;

    private:
        char* data_;
        size_t len_;
        JSContext *context_ = nullptr;
    };

private:
};

class V8_EXPORT Function : public Object {
public:
};


enum PropertyAttribute {
    /** None. **/
    None = 0,
    /** ReadOnly, i.e., not writable. **/
    ReadOnly = 1 << 0,
    /** DontEnum, i.e., not enumerable. **/
    DontEnum = 1 << 1,
    /** DontDelete, i.e., not configurable. **/
    DontDelete = 1 << 2
};

class V8_EXPORT Template : public Data {
public:
    void Set(Isolate* isolate, const char* name, Local<Data> value);
    
    void SetAccessorProperty(Local<Name> name,
                             Local<FunctionTemplate> getter = Local<FunctionTemplate>(),
                             Local<FunctionTemplate> setter = Local<FunctionTemplate>(),
                             PropertyAttribute attribute = None);
    
    std::map<std::string, Local<Data>> fields_;
    
    class AccessorPropertyInfo {
    public:
        Local<FunctionTemplate> getter_;
        Local<FunctionTemplate> setter_;
        PropertyAttribute attribute_;
    };
    
    std::map<Local<Name>, AccessorPropertyInfo> accessor_property_infos_;
};

class V8_EXPORT ObjectTemplate : public Template {
public:
    void SetInternalFieldCount(int value);
    
    int internal_field_count_ = 0;
};

typedef void (*FunctionCallback)(const FunctionCallbackInfo<Value>& info);

class V8_EXPORT FunctionTemplate : public Template {
public:
    static Local<FunctionTemplate> New(
        Isolate* isolate, FunctionCallback callback = nullptr,
        Local<Value> data = Local<Value>());
    
    V8_WARN_UNUSED_RESULT MaybeLocal<Function> GetFunction(
        Local<Context> context);
    
    Local<ObjectTemplate> InstanceTemplate();
    
    void Inherit(Local<FunctionTemplate> parent);
    
    Local<ObjectTemplate> PrototypeTemplate();
    
    V8_INLINE static FunctionTemplate* Cast(v8::Data* obj) {
        return dynamic_cast<FunctionTemplate*>(obj);
    }

    int magic_;
    FunctionCallback callback_;
    Local<Value> data_;
    
    Local<ObjectTemplate> instance_template_;
    Local<ObjectTemplate> prototype_template_;
};

template<typename T>
class ReturnValue {
public:
    //template <class S> V8_INLINE ReturnValue(const ReturnValue<S>& that)
    //  : value_(that.value_) {
    //    static_assert(std::is_base_of<T, S>::value, "type check");
    //}
    // Local setters
    //template <typename S>
    //V8_INLINE void Set(const Global<S>& handle);
    //template <typename S>
    //V8_INLINE void Set(const TracedReferenceBase<S>& handle);
    template <typename S>
    V8_INLINE void Set(const Local<S> handle);
    // Fast primitive setters
    V8_INLINE void Set(bool value);
    V8_INLINE void Set(double i);
    V8_INLINE void Set(int32_t i);
    V8_INLINE void Set(uint32_t i);
    // Fast JS primitive setters
    V8_INLINE void SetNull();
    V8_INLINE void SetUndefined();
    V8_INLINE void SetEmptyString();

    // Pointer setter: Uncompilable to prevent inadvertent misuse.
    template <typename S>
    V8_INLINE void Set(S* whatever);
    
    V8_INLINE ReturnValue(JSContext* context, JSValue* pvalue)
       :context_(context), pvalue_(pvalue) { }
    
    JSContext* context_;
    
    JSValue* pvalue_;
};

template<typename T>
class FunctionCallbackInfo {
public:
    V8_INLINE int Length() const;
    
    V8_INLINE Local<Value> operator[](int i) const;
    
    V8_INLINE Local<Object> This() const {
        Object* obj = new Object();
        obj->u_.value_ = this_;
        return Local<Object>(obj);
    }
    
    //quickjs没有类似的机制S
    V8_INLINE Local<Object> Holder() const {
        return This();
    }
    
    V8_INLINE bool IsConstructCall() const {
        return isConstructCall;
    }
    
    V8_INLINE Local<Value> Data() const {
        return isolate_->GetFunctionTemplate(magic_)->data_;
    }
    
    V8_INLINE Isolate* GetIsolate() const {
        return isolate_;
    }
    
    V8_INLINE ReturnValue<T> GetReturnValue() const {
        return ReturnValue<T>(context_, const_cast<JSValue*>(&value_));
    }

    int argc_;
    JSValueConst *argv_;
    JSValue value_;
    JSContext* context_;
    JSValueConst this_;
    Isolate * isolate_;
    int magic_;
    bool isConstructCall;
};

class ScriptOrigin {
public:
    V8_INLINE ScriptOrigin(
        Local<Value> resource_name) : resource_name_(resource_name) {
    }

    V8_INLINE Local<Value> ResourceName() const {
        return resource_name_;
    }

    Local<Value> resource_name_;
};


class V8_EXPORT HandleScope {
public:
    explicit HandleScope(Isolate* isolate) {
        //do nothing
    }

    ~HandleScope() {
        //do nothing
    }

private:
    void* operator new(size_t size);
    void* operator new[](size_t size);
    void operator delete(void*, size_t);
    void operator delete[](void*, size_t);
};

class V8_EXPORT Script {
public:
    static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
        Local<Context> context, Local<String> source,
        ScriptOrigin* origin = nullptr);

    V8_WARN_UNUSED_RESULT MaybeLocal<Value> Run(Local<Context> context);
    
    ~Script();
private:
    Local<String> source_;
    MaybeLocal<String> resource_name_;
};

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const Local<S> handle) {
    static_assert(std::is_void<T>::value || std::is_base_of<T, S>::value,
                "type check");
    if (V8_UNLIKELY(handle.IsEmpty())) {
        SetUndefined();
    } else if (handle->jsValue_) {
        *pvalue_ = handle->u_.value_;
    } else {
        *pvalue_ = JS_NewStringLen_(context_, handle->u_.str_.data_, handle->u_.str_.len_);
    }
}

template<typename T>
void ReturnValue<T>::Set(double i) {
    static_assert(std::is_base_of<T, Number>::value, "type check");
    *pvalue_ = JS_NewFloat64_(context_, i);
    
}

template<typename T>
void ReturnValue<T>::Set(int32_t i) {
    static_assert(std::is_base_of<T, Integer>::value, "type check");
    *pvalue_ = JS_NewInt32_(context_, i);
}

template<typename T>
void ReturnValue<T>::Set(uint32_t i) {
    static_assert(std::is_base_of<T, Integer>::value, "type check");
    *pvalue_ = JS_NewUint32_(context_, i);
}

template<typename T>
void ReturnValue<T>::Set(bool value) {
    static_assert(std::is_base_of<T, Boolean>::value, "type check");
    if (value) {
        *pvalue_ = JS_True();
    } else {
        *pvalue_ = JS_False();
    }
}

template<typename T>
void ReturnValue<T>::SetNull() {
    static_assert(std::is_base_of<T, Primitive>::value, "type check");
    *pvalue_ = JS_Null();
}

template<typename T>
void ReturnValue<T>::SetUndefined() {
    static_assert(std::is_base_of<T, Primitive>::value, "type check");
    *pvalue_ = JS_Undefined();
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(S* whatever) {
    static_assert(sizeof(S) < 0, "incompilable to prevent inadvertent misuse");
}

template<typename T>
Local<Value> FunctionCallbackInfo<T>::operator[](int i) const {
    JSValue jsVal = JS_Undefined();
    if (i >=0 && i < argc_) {
        jsVal = argv_[i];
    }
    Value* ret = new Value();
    ret->u_.value_ = jsVal;
    return Local<Value>(ret);
}

}  // namespace v8

#endif  // INCLUDE_V8_H_
