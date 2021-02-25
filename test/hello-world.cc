// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <map>
#include <string>
#include <limits>

#include "libplatform/libplatform.h"
#include "v8.h"

#if defined(PLATFORM_WINDOWS)

#if _WIN64
#include "Blob/Win64/SnapshotBlob.h"
#else
#include "Blob/Win32/SnapshotBlob.h"
#endif

#elif defined(PLATFORM_ANDROID_ARM)
#include "Blob/Android/armv7a/SnapshotBlob.h"
#elif defined(PLATFORM_ANDROID_ARM64)
#include "Blob/Android/arm64/SnapshotBlob.h"
#elif defined(PLATFORM_MAC)
#include "Blob/macOS/SnapshotBlob.h"
#elif defined(PLATFORM_IOS)
#include "Blob/iOS/arm64/SnapshotBlob.h"
#endif

template <typename T> inline void __USE(T&&) {}

static void Add(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* Isolate = info.GetIsolate();
    v8::Isolate::Scope IsolateScope(Isolate);
    v8::HandleScope HandleScope(Isolate);
    v8::Local<v8::Context> Context = Isolate->GetCurrentContext();
    v8::Context::Scope ContextScope(Context);

    const char* msg = reinterpret_cast<const char*>((v8::Local<v8::External>::Cast(info.Data()))->Value());
    int32_t a = info[0]->Int32Value(Context).ToChecked();
    int32_t b = info[1]->Int32Value(Context).ToChecked();
    std::cout << "(" << a << "+" << b << "), msg:" << msg << std::endl;

    info.GetReturnValue().Set(a + b);
}

static void Print(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    
    if (info[0]->IsBigInt()) {
        auto bigint = info[0].As<v8::BigInt>();
        std::cout << "bigint: " << bigint->Int64Value() << std::endl;
    } else {
        std::string key = *(v8::String::Utf8Value(isolate, info[0]->ToString(context).ToLocalChecked()));
        std::cout << "js message: " << key << std::endl;
    }
}

static void OnGarbageCollected(const v8::WeakCallbackInfo<v8::Global<v8::Object>>& Data)
{
    auto map = static_cast<std::map<std::string, std::string>*>(Data.GetInternalField(0));
    v8::Global<v8::Object> *self = Data.GetParameter();
    std::cout << "gc map=" << Data.GetInternalField(0)<< ",v8::Global=" << self << std::endl;
    std::cout << "map.size=" << map->size() << std::endl;
    self->Reset();
    delete map;
    delete Data.GetParameter();
}

static void NewMap(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> Context = isolate->GetCurrentContext();

    if (info[0]->IsExternal()) {
        void* map = v8::Local<v8::External>::Cast(info[0])->Value();
        info.This()->SetAlignedPointerInInternalField(0, map);
        std::cout << "just bind, map=" << map << std::endl;
    } else {
        std::map<std::string, std::string>* map = new std::map<std::string, std::string>();
        info.This()->SetAlignedPointerInInternalField(0, map);
        v8::Global<v8::Object> *self = new v8::Global<v8::Object>(isolate, info.This());
        std::cout << "map=" << map << ",v8::Global=" << self <<  std::endl;
        (*self).SetWeak<v8::Global<v8::Object>>(self, OnGarbageCollected, v8::WeakCallbackType::kInternalFields);
    }
}

static void MapBaseMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    std::map<std::string, std::string>* map = static_cast<std::map<std::string, std::string>*>(info.Holder()->GetAlignedPointerFromInternalField(0));
    
    std::cout << "MapBaseMethod: map=" << map << ", size=" << map->size() <<  std::endl;
}

static void MapGet(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    std::map<std::string, std::string>* map = static_cast<std::map<std::string, std::string>*>(info.Holder()->GetAlignedPointerFromInternalField(0));
    std::string key = *(v8::String::Utf8Value(isolate, info[0]->ToString(context).ToLocalChecked()));

    auto iter = map->find(key);

    if (iter == map->end()) return;

    const std::string& value = (*iter).second;
    info.GetReturnValue().Set(
        v8::String::NewFromUtf8(info.GetIsolate(), value.c_str(),
            v8::NewStringType::kNormal,
            static_cast<int>(value.length())).ToLocalChecked());
}

static void MapSet(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    std::map<std::string, std::string>* map = static_cast<std::map<std::string, std::string>*>(info.Holder()->GetAlignedPointerFromInternalField(0));
    std::string key = *(v8::String::Utf8Value(isolate, info[0]->ToString(context).ToLocalChecked()));
    std::string val = *(v8::String::Utf8Value(isolate, info[1]->ToString(context).ToLocalChecked()));

    (*map)[key] = val;
}

static void MapCount(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    std::map<std::string, std::string>* map = static_cast<std::map<std::string, std::string>*>(info.Holder()->GetAlignedPointerFromInternalField(0));
    
    info.GetReturnValue().Set((int)map->size());
}

static void MapCount2(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    std::map<std::string, std::string>* map = static_cast<std::map<std::string, std::string>*>(info.Holder()->GetAlignedPointerFromInternalField(0));
    
    const char* msg = reinterpret_cast<const char*>((v8::Local<v8::External>::Cast(info.Data()))->Value());
    std::cout << "MapCount2, msg:" << msg << std::endl;
    info.GetReturnValue().Set((int)map->size());
}

static void MapCountSet(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    std::map<std::string, std::string>* map = static_cast<std::map<std::string, std::string>*>(info.Holder()->GetAlignedPointerFromInternalField(0));
    
    std::cout << "ignore set count, " << info[0]->Int32Value(context).ToChecked() << std::endl;
}

static void MapCountSet2(v8::Local<v8::Name> property,v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    
    const char* msg = reinterpret_cast<const char*>((v8::Local<v8::External>::Cast(info.Data()))->Value());
    std::cout << "MapCountSet2, msg:" << msg << ", val:" << value->Int32Value(context).ToChecked() << std::endl;
}

static void MapStatic(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    
    std::cout << "map static, " << info[0]->Int32Value(context).ToChecked() << std::endl;
}

static void MapStaticPropGet(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    
    std::cout << "MapStaticPropGet" << std::endl;
    
    info.GetReturnValue().Set(999);
}

static void MapStaticPropSet(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    
    std::cout << "MapStaticPropSet " << info[0]->Int32Value(context).ToChecked() << std::endl;
    isolate->ThrowException(v8::Exception::Error(info[0]->ToString(context).ToLocalChecked()));
}

static void JustThrow(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    
    isolate->ThrowException(v8::Exception::Error(info[0]->ToString(context).ToLocalChecked()));
}

static void GetInt64(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    
    int32_t t = info[0]->Int32Value(context).ToChecked();
    v8::Local<v8::Value> ret;
    if (t == 0) {
        //std::cout << "std::numeric_limits<int64_t>::max():" << std::numeric_limits<int64_t>::max() << std::endl;
        ret = v8::BigInt::New(isolate, std::numeric_limits<int64_t>::max());
    } else if (t == 1) {
        //std::cout << "std::numeric_limits<int64_t>::min():" << std::numeric_limits<int64_t>::min() << std::endl;
        ret = v8::BigInt::New(isolate, std::numeric_limits<int64_t>::min());
    } else {
        //std::cout << "std::numeric_limits<uint64_t>::max():" << static_cast<int64_t>(std::numeric_limits<uint64_t>::max()) << std::endl;
        ret = v8::BigInt::NewFromUnsigned(isolate, std::numeric_limits<uint64_t>::max());
    }
    info.GetReturnValue().Set(ret);
}

static v8::Local<v8::Value> NewStringScope(v8::Isolate* isolate) {
    //v8::HandleScope handle_scope(isolate); //expect crash
    //return v8::String::NewFromUtf8(isolate, "NewStringScope").ToLocalChecked();
    v8::EscapableHandleScope handle_scope(isolate);
    return handle_scope.Escape(v8::String::NewFromUtf8(isolate, "NewStringScope").ToLocalChecked());
}

static void TestScope(const v8::FunctionCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(NewStringScope(info.GetIsolate()));
}

//static v8::Local<v8::FunctionTemplate> GenTemplate(v8::Isolate* isolate) {
//    v8::EscapableHandleScope handle_scope(isolate);
//    return handle_scope.Escape(v8::FunctionTemplate::New(isolate));
//}

int main(int argc, char* argv[]) {
    // Initialize V8.
    v8::StartupData SnapshotBlob;
    SnapshotBlob.data = (const char *)SnapshotBlobCode;
    SnapshotBlob.raw_size = sizeof(SnapshotBlobCode);
    v8::V8::SetSnapshotDataBlob(&SnapshotBlob);

    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

    // Create a new Isolate and make it the current one.
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    {
        v8::Isolate::Scope isolate_scope(isolate);

        // Create a stack-allocated handle scope.
        v8::HandleScope handle_scope(isolate);

        // Create a new context.
        v8::Local<v8::Context> context = v8::Context::New(isolate);

        // Enter the context for compiling and running the hello world script.
        v8::Context::Scope context_scope(context);
        
        auto external = v8::External::New(isolate, (void *)"ExternalInfo...");
        context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "g_number").ToLocalChecked(), v8::Number::New(isolate, 1999.99)).Check();
        context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "native_add").ToLocalChecked(),
            v8::FunctionTemplate::New(isolate, Add, external)->GetFunction(context).ToLocalChecked())
            .Check();

        auto print = v8::FunctionTemplate::New(isolate, Print)->GetFunction(context).ToLocalChecked();
        context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "print").ToLocalChecked(), print).Check();
        context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "print").ToLocalChecked(), print).Check();//一个local变量多次Set
        
        context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "getbigint").ToLocalChecked(),
            v8::FunctionTemplate::New(isolate, GetInt64)->GetFunction(context).ToLocalChecked())
            .Check();
        
        context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "justthrow").ToLocalChecked(),
            v8::FunctionTemplate::New(isolate, JustThrow)->GetFunction(context).ToLocalChecked())
            .Check();
        
        context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "testscope").ToLocalChecked(),
            v8::FunctionTemplate::New(isolate, TestScope)->GetFunction(context).ToLocalChecked())
            .Check();
        
        auto base_tpl = v8::FunctionTemplate::New(isolate, NewMap);
        base_tpl->InstanceTemplate()->SetInternalFieldCount(1);
        base_tpl->PrototypeTemplate()->Set(isolate, "basemethod", v8::FunctionTemplate::New(isolate, MapBaseMethod));

        auto tpl = v8::FunctionTemplate::New(isolate, NewMap);
        tpl->InstanceTemplate()->SetInternalFieldCount(1);
        tpl->PrototypeTemplate()->Set(isolate, "get", v8::FunctionTemplate::New(isolate, MapGet));
        tpl->PrototypeTemplate()->Set(isolate, "set", v8::FunctionTemplate::New(isolate, MapSet));
        tpl->PrototypeTemplate()->SetAccessorProperty(v8::String::NewFromUtf8(isolate, "count").ToLocalChecked(), v8::FunctionTemplate::New(isolate, MapCount),
                                                      v8::FunctionTemplate::New(isolate, MapCountSet));
        tpl->PrototypeTemplate()->SetAccessor(v8::String::NewFromUtf8(isolate, "count2").ToLocalChecked(), MapCount2, MapCountSet2, external);
        tpl->Set(isolate, "static", v8::FunctionTemplate::New(isolate, MapStatic));
        tpl->SetAccessorProperty(v8::String::NewFromUtf8(isolate, "sprop").ToLocalChecked(), v8::FunctionTemplate::New(isolate, MapStaticPropGet),
                                 v8::FunctionTemplate::New(isolate, MapStaticPropSet));
        
        tpl->Inherit(base_tpl);

        context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "map").ToLocalChecked(), tpl->GetFunction(context).ToLocalChecked()).Check();

        {
            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, "'Hello' + ', World!'",
                    v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

            // Convert the result to an UTF8 string and print it.
            v8::String::Utf8Value utf8(isolate, result);
            printf("%s\n", *utf8);
        }

        {
            // Use the JavaScript API to generate a WebAssembly module.
            //
            // |bytes| contains the binary format for the following module:
            //
            //     (func (export "add") (param i32 i32) (result i32)
            //       get_local 0
            //       get_local 1
            //       i32.add)
            //
            const char* csource = R"(
                function add(x, y) {
                    return x + y;
                }
                add(3, 4);
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

            // Convert the result to a uint32 and print it.
            uint32_t number = result->Uint32Value(context).ToChecked();
            printf("3 + 4 = %u\n", number);
        }

        {
            const char* csource = R"(
                var patt = /runoob/i;
                native_add(5, 6);
                //g_number;
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

            // Convert the result to a uint32 and print it.
            uint32_t number = result->Uint32Value(context).ToChecked();
            printf("native_add(5, 6) = %u\n", number);
        }

        {
            const char* csource = R"(
                let m = new map();
                print(m.count);
                m.set('abc', '123');
                m.set('def', '4567');
                print(m.count);
                print(m.get('abc'));
                print(m.get('def'));
                print(m.get('fff'));
                m.count = 1000;
            
                print(m.count2);
                m.count2 = 1001;
            
                m.basemethod();
            
                map.static(1024);
                print(map.sprop);
                m = undefined;
                
                print(g_map.count);
                print(g_map.get('test'));
                g_map = undefined;
              )";

            std::map<std::string, std::string> stack_map;
            stack_map["test"] = "ok";
            //绑定的方案1
            auto bindto = v8::External::New(context->GetIsolate(), &stack_map);
            v8::Local<v8::Value> args[] = { bindto };
            auto binded = tpl->GetFunction(context).ToLocalChecked()->NewInstance(context, 1, args);
            context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "g_map").ToLocalChecked(), binded.ToLocalChecked()).Check();
            
            std::cout << "is tpl ? " << tpl->HasInstance(binded.ToLocalChecked()) << std::endl;
            std::cout << "is basetpl ? " << base_tpl->HasInstance(binded.ToLocalChecked()) << std::endl;
            auto math = context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "Math").ToLocalChecked()).ToLocalChecked().As<v8::Object>();
            std::cout << "Math is tpl ? " << tpl->HasInstance(math) << std::endl;
            std::cout << "Math is basetpl ? " << base_tpl->HasInstance(math) << std::endl;
            
            //绑定的方案2
            //auto stack_map_obj = tpl->InstanceTemplate()->NewInstance(context).ToLocalChecked();
            //stack_map_obj->SetAlignedPointerInInternalField(0, &stack_map);
            //context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "g_map").ToLocalChecked(), stack_map_obj).Check();


            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            script->Run(context).ToLocalChecked();

            std::cout << "LowMemoryNotification.." << std::endl;
            isolate->LowMemoryNotification();
        }
        
        //exception
        {
            const char* csource[] = {R"(
                //throw "abc";
                new map111();
                print('!!!!!must not be print');
                )",
                R"(
                justthrow('throw???');
                print('!!!!!must not be print');
                )"
                ,
                R"(
                map.sprop = 888;
                print('!!!!!must not be print');
                )"
            };

            for(int i = 0; i < sizeof(csource)/sizeof(csource[0]); i++) {
                // Create a string containing the JavaScript source code.
                v8::Local<v8::String> source =
                    v8::String::NewFromUtf8(isolate, csource[i], v8::NewStringType::kNormal)
                    .ToLocalChecked();
                
                v8::TryCatch tryCatch(isolate);

                // Compile the source code.
                v8::Local<v8::Script> script =
                    v8::Script::Compile(context, source).ToLocalChecked();

                std::cout << "---------------exception test " << i << " start------------------" << std::endl;
                // Run the script to get the result.
                auto ret = script->Run(context);
                
                if (tryCatch.HasCaught()) {
                    v8::String::Utf8Value info(isolate, tryCatch.Exception());
                    std::cout << "exception catch, info: " << *info << std::endl;
                    auto msg = tryCatch.Message();
                    std::cout << "fileinfo:" << *v8::String::Utf8Value(isolate, msg->GetScriptResourceName()) << ":" << msg->GetLineNumber(context).ToChecked() << std::endl;
                    v8::Local<v8::Value> stackTrace;
                    if (tryCatch.StackTrace(context).ToLocal(&stackTrace) &&
                        stackTrace->IsString()) {
                        v8::String::Utf8Value stack(isolate, stackTrace);
                        std::cout << "--- " << *stack << std::endl;
                    }
                }
                std::cout << "---------------exception test " << i << " end ------------------" << std::endl;
            }
        }

        //call js function
        {
            const char* csource = R"(
                function jsfunc(x, y) {
                    print('jsfunc called, x=' + x + ',y=' + y);
                    return x + y;
                }
                jsfunc;
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
            if (result->IsFunction()) {
                auto Func = result.As<v8::Function>();
                std::vector<v8::Local<v8::Value>> Argv;
                Argv.push_back(v8::Number::New(isolate, 1));
                Argv.push_back(v8::Number::New(isolate, 9));
                auto ret1 = Func->Call(context, v8::Undefined(isolate), Argv.size(), Argv.data()).ToLocalChecked();
                std::cout << "ret1 =" << *v8::String::Utf8Value(isolate, ret1) << std::endl;

                Argv.clear();
                Argv.push_back(v8::String::NewFromUtf8(isolate, "john").ToLocalChecked());
                Argv.push_back(v8::String::NewFromUtf8(isolate, "che").ToLocalChecked());

                auto ret2 = Func->Call(context, v8::Undefined(isolate), Argv.size(), Argv.data()).ToLocalChecked();
                std::cout << "ret2 =" << *v8::String::Utf8Value(isolate, ret2) << std::endl;
            }
        }
        
        //bigint
        {
            const char* csource = R"(
                const b0 = getbigint(0);
                const b1 = getbigint(1);
                const b2 = getbigint(2);
                print(typeof b0);
                print(b0);
                print(b1);
                print(b2);
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            script->Run(context).ToLocalChecked();
        }
        
        //arraybuffer
        {
            auto ab1 = v8::ArrayBuffer::New(isolate, 3);
            uint8_t * arr = (uint8_t*)ab1->GetContents().Data();
            arr[0] = 1;
            arr[1] = 2;
            arr[2] = 3;
            
            uint8_t arr2[] = {4, 5, 6, 7, 8, 9};
            
            auto ab2 = v8::ArrayBuffer::New(isolate, arr2, sizeof(arr2));
            
            context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "ab1").ToLocalChecked(), ab1).Check();
            //qucikjs new的对象，只能用一次，不能多次设置，否则会触发引用计数小于0的报错，这点和v8不太一样，可以考虑在Escape那增加饮用计数
            //context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "ab2").ToLocalChecked(), ab1).Check();
            context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "ab2").ToLocalChecked(), ab2).Check();

            const char* csource = R"(
                let u1 = new Uint8Array(ab1);
                let u2 = new Uint8Array(ab2);
                print(u1.length);
                print(u2.length);
                print(u1);
                print(u2);
                //u1;
                new Uint16Array(ab2, 2, 2);
                //new DataView(ab2);
              )";
            
            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            auto ret = script->Run(context).ToLocalChecked();
            if (ret->IsArrayBufferView()) {
                auto dataview = ret.As<v8::ArrayBufferView>();
                auto dvc = dataview->Buffer()->GetContents();
                std::cout << "View->ByteLength(): " << dataview->ByteLength() << "," << dataview->ByteOffset() << std::endl;
                std::cout << "dataview: " << dvc.Data() << "," << dvc.ByteLength() << std::endl;
            }
        }
        
        //date
        {
            context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "gdate").ToLocalChecked(), v8::Date::New(context, 999999999999.99).ToLocalChecked()).Check();
            const char* csource = R"(
                print(gdate);
                new Date();
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            auto ret = script->Run(context).ToLocalChecked();
            if (ret->IsDate()) {
                std::cout << "date.valueof:" << ret.As<v8::Date>()->ValueOf() << std::endl;
            }
        }
        //obj.Get
        {
            std::cout << "find print? " << context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "print").ToLocalChecked()).ToLocalChecked()->IsFunction() << std::endl;
            std::cout << "find jsfunc? " << context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "jsfunc").ToLocalChecked()).ToLocalChecked()->IsFunction() << std::endl;
            std::cout << "find gdate? " << context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "gdate").ToLocalChecked()).ToLocalChecked()->IsDate() << std::endl;
            std::cout << "map111 == undefined? " << context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "map111").ToLocalChecked()).ToLocalChecked()->IsUndefined() << std::endl;
            std::cout << "find patt? " << context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "patt").ToLocalChecked()).ToLocalChecked()->IsRegExp() << std::endl;
            std::cout << "gdate is regexp ? " << context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "gdate").ToLocalChecked()).ToLocalChecked()->IsRegExp() << std::endl;
        }
        
        //promise
        {
            isolate->SetPromiseRejectCallback([](v8::PromiseRejectMessage message) {
                if (message.GetEvent() == v8::kPromiseRejectWithNoHandler) {
                    std::cout << "kPromiseRejectWithNoHandler:" << *v8::String::Utf8Value(message.GetPromise()->GetIsolate(), message.GetValue())  << std::endl;
                } else if (message.GetEvent() == v8::kPromiseHandlerAddedAfterReject) {
                    std::cout << "kPromiseRejectAfterResolved"  << std::endl;
                } else {
                    std::cout << "unknw event: " << message.GetEvent() << std::endl;
                }
            });
            
            const char* csource[] = {R"(
                new Promise((resolve, reject)=>{
                    throw 'unhandled rejection'
                })
            )", R"(
                new Promise((resolve, reject)=>{
                    throw 'unhandled rejection'
                }).catch(error => {
                })
            )"};

            for(int i = 0; i < sizeof(csource)/sizeof(csource[0]); i++) {
                std::cout << "---------------promise test " << i << " start------------------" << std::endl;
                // Create a string containing the JavaScript source code.
                v8::Local<v8::String> source =
                    v8::String::NewFromUtf8(isolate, csource[i], v8::NewStringType::kNormal)
                    .ToLocalChecked();

                // Compile the source code.
                v8::Local<v8::Script> script =
                    v8::Script::Compile(context, source).ToLocalChecked();

                // Run the script to get the result.
                auto ret = script->Run(context).ToLocalChecked();
                std::cout << "---------------promise test " << i << " end------------------" << std::endl;
            }
        }
        
        //date
        {
            const char* csource = R"(
                print(testscope());
                /*let ooo = new map();
                let ppp = Object.getOwnPropertyDescriptor(map.prototype, 'count')
                let j =0;
                for(var i in ppp) {
                    print(`-----------------------${j++}--------------------------`)
                    print(i);
                    print(ppp[i]);
                }*/
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            auto ret = script->Run(context).ToLocalChecked();
        }
        
        //GetOwnPropertyNames
        {
            const char* csource = R"(
                gobj1 = {'k1': 1, 'k2': 2}
                gobj2 = {}
                Object.setPrototypeOf(gobj1, {'k3':3});
                print('gobj1.k3(1):' + gobj1.k3);
                print('gobj2.k3(1):' +gobj2.k3);
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            auto ret = script->Run(context).ToLocalChecked();
            
            auto printProperty = [&](const char* objName) {
                auto obj = context->Global()->Get(context, v8::String::NewFromUtf8(isolate, objName).ToLocalChecked()).ToLocalChecked().As<v8::Object>();
                
                auto propertyNames = obj->GetOwnPropertyNames(context).ToLocalChecked();
                
                std::cout << objName << " property count: " << propertyNames->Length() << std::endl;
                
                for (int i = 0; i < propertyNames->Length(); i++) {
                    auto key = propertyNames->Get(context, i).ToLocalChecked();
                    std::cout << i << " : " << *(v8::String::Utf8Value(isolate, key)) << std::endl;
                }
            };
            
            printProperty("gobj1");
            printProperty("gobj2");
            //printProperty("Math");
            
            auto obj1 = context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "gobj1").ToLocalChecked()).ToLocalChecked().As<v8::Object>();
            std::cout << "HasOwnProperty('k3'): " << obj1->HasOwnProperty(context, v8::String::NewFromUtf8(isolate, "k3", v8::NewStringType::kNormal).ToLocalChecked()).ToChecked() << std::endl;
            std::cout << "HasOwnProperty('k2'): " << obj1->HasOwnProperty(context, v8::String::NewFromUtf8(isolate, "k2", v8::NewStringType::kNormal).ToLocalChecked()).ToChecked() << std::endl;
            auto obj2 = context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "gobj2").ToLocalChecked()).ToLocalChecked().As<v8::Object>();
            auto r = obj2->SetPrototype(context, obj1->GetPrototype());
            
            const char* csource2 = R"(
                print('gobj1.k3(2):' + gobj1.k3);
                print('gobj2.k3(2):' + gobj2.k3);
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source2 =
                v8::String::NewFromUtf8(isolate, csource2, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script2 =
                v8::Script::Compile(context, source2).ToLocalChecked();

            // Run the script to get the result.
            ret = script2->Run(context).ToLocalChecked();
            
        }
        
        //Map
        {
            v8::Local<v8::Map> map = v8::Map::New(isolate);
            
            __USE(context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "gmap").ToLocalChecked(), map));
            
            __USE(map->Set(context, v8::String::NewFromUtf8(isolate, "k").ToLocalChecked(), v8::String::NewFromUtf8(isolate, "kk").ToLocalChecked()));
            
            const char* csource = R"(
                print(gmap instanceof Map);
                print('k:' + gmap.get('k'));
                gmap.set('kk', 'kkk')
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            __USE(script->Run(context).ToLocalChecked());
            
            auto val = map->Get(context, v8::String::NewFromUtf8(isolate, "kk").ToLocalChecked()).ToLocalChecked();
            
            std::cout << "kk set by script: " << *(v8::String::Utf8Value(isolate, val)) << std::endl;
            
            map->Clear();
            
            const char* csource2 = R"(
                print('k:' + gmap.get('k'));
                print('kk:' + gmap.get('kk'));
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source2 =
                v8::String::NewFromUtf8(isolate, csource2, v8::NewStringType::kNormal)
                .ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script2 =
                v8::Script::Compile(context, source2).ToLocalChecked();

            // Run the script to get the result.
            __USE(script2->Run(context).ToLocalChecked());
        }
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete create_params.array_buffer_allocator;
    return 0;
}
