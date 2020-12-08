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

    std::map<std::string, std::string>* map = new std::map<std::string, std::string>();
    info.This()->SetAlignedPointerInInternalField(0, map);
    v8::Global<v8::Object> *self = new v8::Global<v8::Object>(isolate, info.This());
    std::cout << "map=" << map << ",v8::Global=" << self <<  std::endl;
    (*self).SetWeak<v8::Global<v8::Object>>(self, OnGarbageCollected, v8::WeakCallbackType::kInternalFields);
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

static void MapCountSet(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    std::map<std::string, std::string>* map = static_cast<std::map<std::string, std::string>*>(info.Holder()->GetAlignedPointerFromInternalField(0));
    
    std::cout << "ignore set count, " << info[0]->Int32Value(context).ToChecked() << std::endl;
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

        context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "print").ToLocalChecked(),
            v8::FunctionTemplate::New(isolate, Print)->GetFunction(context).ToLocalChecked())
            .Check();
        
        context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "getbigint").ToLocalChecked(),
            v8::FunctionTemplate::New(isolate, GetInt64)->GetFunction(context).ToLocalChecked())
            .Check();

        auto tpl = v8::FunctionTemplate::New(isolate, NewMap);
        tpl->InstanceTemplate()->SetInternalFieldCount(1);
        tpl->PrototypeTemplate()->Set(isolate, "get", v8::FunctionTemplate::New(isolate, MapGet));
        tpl->PrototypeTemplate()->Set(isolate, "set", v8::FunctionTemplate::New(isolate, MapSet));
        tpl->PrototypeTemplate()->SetAccessorProperty(v8::String::NewFromUtf8(isolate, "count").ToLocalChecked(), v8::FunctionTemplate::New(isolate, MapCount),
                                                      v8::FunctionTemplate::New(isolate, MapCountSet));
        tpl->Set(isolate, "static", v8::FunctionTemplate::New(isolate, MapStatic));
        tpl->SetAccessorProperty(v8::String::NewFromUtf8(isolate, "sprop").ToLocalChecked(), v8::FunctionTemplate::New(isolate, MapStaticPropGet),
                                 v8::FunctionTemplate::New(isolate, MapStaticPropSet));

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
                map.static(1024);
                print(map.sprop);
                map.sprop = 888;
                m = undefined;
                
                //print(g_map.count);
                //print(g_map.get('test'));
                //g_map = undefined;
              )";

            //std::map<std::string, std::string> stack_map;
            //stack_map["test"] = "ok";
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
            const char* csource = R"(
                //throw "abc";
                new map111();
              )";

            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, csource, v8::NewStringType::kNormal)
                .ToLocalChecked();
            
            v8::TryCatch tryCatch(isolate);

            // Compile the source code.
            v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();

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
        
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete create_params.array_buffer_allocator;
    return 0;
}
