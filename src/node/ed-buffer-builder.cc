/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "ed-buffer-builder.h"
#include "ed-buffer.h"
#include "ed-buffer-string.h"

#include <cstring>

EdBufferBuilder::EdBufferBuilder()
{
    this->actual_ = new edcore::BufferBuilder();
}

edcore::Buffer *EdBufferBuilder::BuildBuffer()
{
    return this->actual_->build();
}

EdBufferBuilder::~EdBufferBuilder()
{
    delete this->actual_;
}

void EdBufferBuilder::AcceptChunk(const v8::FunctionCallbackInfo<v8::Value> &args)
{
    v8::Isolate *isolate = args.GetIsolate();
    EdBufferBuilder *obj = ObjectWrap::Unwrap<EdBufferBuilder>(args.Holder());

    if (!args[0]->IsString())
    {
        isolate->ThrowException(v8::Exception::TypeError(
            v8::String::NewFromUtf8(isolate, "Argument must be a string")));
        return;
    }

    v8::Local<v8::String> chunk = v8::Local<v8::String>::Cast(args[0]);
    edcore::BufferString *str = new v8StringAsBufferString(chunk);
    obj->actual_->acceptChunk(str);
    delete str;
}

void EdBufferBuilder::Finish(const v8::FunctionCallbackInfo<v8::Value> &args)
{
    EdBufferBuilder *obj = ObjectWrap::Unwrap<EdBufferBuilder>(args.Holder());
    obj->actual_->finish();
}

void EdBufferBuilder::Build(const v8::FunctionCallbackInfo<v8::Value> &args)
{
    v8::Local<v8::Object> result = EdBuffer::Create(args.GetIsolate(), args.Holder());
    args.GetReturnValue().Set(result);
}

void EdBufferBuilder::New(const v8::FunctionCallbackInfo<v8::Value> &args)
{
    v8::Isolate *isolate = args.GetIsolate();

    if (args.IsConstructCall())
    {
        // Invoked as constructor: `new MyObject(...)`
        EdBufferBuilder *obj = new EdBufferBuilder();
        obj->Wrap(args.This());
        args.GetReturnValue().Set(args.This());
    }
    else
    {
        // Invoked as plain function `MyObject(...)`, turn into construct call.
        const int argc = 1;
        v8::Local<v8::Value> argv[argc] = {args[0]};
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
        v8::Local<v8::Object> result =
            cons->NewInstance(context, argc, argv).ToLocalChecked();
        args.GetReturnValue().Set(result);
    }
}

void EdBufferBuilder::Init(v8::Local<v8::Object> exports)
{
    v8::Isolate *isolate = exports->GetIsolate();

    // Prepare constructor template
    v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, New);
    tpl->SetClassName(v8::String::NewFromUtf8(isolate, "EdBufferBuilder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Prototype
    NODE_SET_PROTOTYPE_METHOD(tpl, "AcceptChunk", AcceptChunk);
    NODE_SET_PROTOTYPE_METHOD(tpl, "Finish", Finish);
    NODE_SET_PROTOTYPE_METHOD(tpl, "Build", Build);

    constructor.Reset(isolate, tpl->GetFunction());
    exports->Set(v8::String::NewFromUtf8(isolate, "EdBufferBuilder"),
                 tpl->GetFunction());
}
