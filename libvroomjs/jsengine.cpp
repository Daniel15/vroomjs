// This file is part of the VroomJs library.
//
// Author:
//     Federico Di Gregorio <fog@initd.org>
//
// Copyright © 2013 Federico Di Gregorio <fog@initd.org>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "vroomjs.h"

using namespace v8;

static Handle<Value> managed_prop_get(Local<String> name, const AccessorInfo& info)
{
    Local<Object> self = info.Holder();
    Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
    ManagedRef* ref = (ManagedRef*)wrap->Value();
    return ref->GetPropertyValue(name);
}

static Handle<Value> managed_prop_set(Local<String> name, Local<Value> value, const AccessorInfo& info)
{

}

JsEngine* JsEngine::New()
{
    JsEngine* engine = new JsEngine();
    if (engine != NULL) {            
        engine->isolate_ = Isolate::New();
        Locker locker(engine->isolate_);
        Isolate::Scope isolate_scope(engine->isolate_);
        engine->context_ = new Persistent<Context>(Context::New());
        
        (*(engine->context_))->Enter();
        
        // Setup the template we'll use for all managed object references.
        HandleScope scope;            
        Handle<ObjectTemplate> o = ObjectTemplate::New();
        o->SetInternalFieldCount(1);
        o->SetNamedPropertyHandler(managed_prop_get, managed_prop_set);
        Persistent<ObjectTemplate> p = Persistent<ObjectTemplate>::New(o);
        engine->managed_template_ = new Persistent<ObjectTemplate>(p);
    }
    
    return engine;
}

void JsEngine::Dispose()
{
    {
        Locker locker(isolate_);
        Isolate::Scope isolate_scope(isolate_);
        managed_template_->Dispose();
        delete managed_template_;
        context_->Dispose();            
        delete context_;
    }

    isolate_->Dispose();
}

jsvalue JsEngine::Execute(const uint16_t* str)
{
    jsvalue v;

    Locker locker(isolate_);
    Isolate::Scope isolate_scope(isolate_);
    (*(context_))->Enter();
        
    HandleScope scope;
        
    Handle<String> source = String::New(str);

    TryCatch trycatch;
    
    Handle<Script> script = Script::Compile(source);          
    if (!script.IsEmpty()) {
        Local<Value> result = script->Run();
        if (result.IsEmpty())
            v = ErrorFromV8(trycatch);
        else
            v = AnyFromV8(result);        
    }
    else {
        v = ErrorFromV8(trycatch);
    }
            
    (*(context_))->Exit();

    return v;     
}

jsvalue JsEngine::SetValue(const uint16_t* name, jsvalue value)
{
    Locker locker(isolate_);
    Isolate::Scope isolate_scope(isolate_);
    (*(context_))->Enter();
        
    HandleScope scope;
        
    Handle<Value> v = AnyToV8(value);

    (*(context_))->Global()->Set(String::New(name), v);          
    (*(context_))->Exit();
    
    return AnyFromV8(Null());
}

jsvalue JsEngine::GetValue(const uint16_t* name)
{
    Locker locker(isolate_);
    Isolate::Scope isolate_scope(isolate_);
    (*(context_))->Enter();
        
    HandleScope scope;
        
    (*(context_))->Exit();
    
    return AnyFromV8(Null());
}

jsvalue JsEngine::ErrorFromV8(TryCatch& trycatch)
{
    jsvalue v;

    Handle<Value> exception = trycatch.Exception();
    v = StringFromV8(exception);
    v.type = JSVALUE_TYPE_ERROR;
    
    return v;
}
    
jsvalue JsEngine::StringFromV8(Handle<Value> value)
{
    jsvalue v;
    
    Local<String> s = value->ToString();
    v.length = s->Length();
    v.value.str = new uint16_t[v.length+1];
    if (v.value.str != NULL) {
        s->Write(v.value.str);
        v.type = JSVALUE_TYPE_STRING;
    }

    return v;
}    
    
jsvalue JsEngine::AnyFromV8(Handle<Value> value)
{
    jsvalue v;
    
    // Initialize to a generic error.
    v.type = JSVALUE_TYPE_ERROR;
    v.length = 0;
    v.value.str = 0;
    
    if (value->IsNull() || value->IsUndefined()) {
        v.type = JSVALUE_TYPE_NULL;
    }                
    else if (value->IsBoolean()) {
        v.type = JSVALUE_TYPE_BOOLEAN;
        v.value.i32 = value->BooleanValue() ? 1 : 0;
    }
    else if (value->IsInt32()) {
        v.type = JSVALUE_TYPE_INTEGER;
        v.value.i32 = value->Int32Value();            
    }
    else if (value->IsNumber()) {
        v.type = JSVALUE_TYPE_NUMBER;
        v.value.num = value->NumberValue();
    }
    else if (value->IsString()) {
        v = StringFromV8(value);
    }
    else if (value->IsDate()) {
        v.type = JSVALUE_TYPE_DATE;
        v.value.num = value->NumberValue();
    }
    else if (value->IsArray()) {
        Handle<Array> object = Handle<Array>::Cast(value->ToObject());
        v.length = object->Length();
        jsvalue* array = new jsvalue[v.length];
        if (array != NULL) {
            for(int i = 0; i < v.length; i++) {
                array[i] = AnyFromV8(object->Get(i));
            }
            v.type = JSVALUE_TYPE_ARRAY;
            v.value.arr = array;
        }
    }

    return v;
}

Handle<Value> JsEngine::AnyToV8(jsvalue v)
{
    if (v.type == JSVALUE_TYPE_NULL) {
        return Null();
    }
    if (v.type == JSVALUE_TYPE_BOOLEAN) {
        return Boolean::New(v.value.i32);
    }
    if (v.type == JSVALUE_TYPE_INTEGER) {
        return Int32::New(v.value.i32);
    }
    if (v.type == JSVALUE_TYPE_NUMBER) {
        return Number::New(v.value.num);
    }
    if (v.type == JSVALUE_TYPE_STRING) {
        return String::New(v.value.str);
    }
    if (v.type == JSVALUE_TYPE_DATE) {
        return Date::New(v.value.num);
    }
    
    // This is an ID to a managed object that lives inside the JsEngine keep-alive
    // cache. We just wrap it and the pointer to the engine inside an External.
    
    if (v.type == JSVALUE_TYPE_MANAGED) {
        ManagedRef* ref = new ManagedRef(this, v.value.i32);
        Local<Object> obj = (*(managed_template_))->NewInstance();
        obj->SetInternalField(0, External::New(ref));
        return obj;
    }

    return Null();
}