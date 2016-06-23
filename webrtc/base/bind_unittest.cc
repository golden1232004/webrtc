/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#if defined(UNIT_TEST)
#include "webrtc/base/bind.h"
#include "webrtc/base/gunit.h"

#include "webrtc/base/refcount.h"

namespace rtc {

namespace {

struct LifeTimeCheck;

struct MethodBindTester {
  void NullaryVoid() { ++call_count; }
  int NullaryInt() { ++call_count; return 1; }
  int NullaryConst() const { ++call_count; return 2; }
  void UnaryVoid(int dummy) { ++call_count; }
  template <class T> T Identity(T value) { ++call_count; return value; }
  int UnaryByRef(int& value) const { ++call_count; return ++value; }  // NOLINT
  int Multiply(int a, int b) const { ++call_count; return a * b; }
  void RefArgument(const scoped_refptr<LifeTimeCheck>& object) {
    EXPECT_TRUE(object.get() != nullptr);
  }

  mutable int call_count;
};

struct A { int dummy; };
struct B: public RefCountInterface { int dummy; };
struct C: public A, B {};
struct D {
  int AddRef();
};
struct E: public D {
  int Release();
};
struct F {
  void AddRef();
  void Release();
};

struct LifeTimeCheck {
  LifeTimeCheck() : ref_count_(0) {}
  void AddRef() { ++ref_count_; }
  void Release() { --ref_count_; }
  void NullaryVoid() {}
  int ref_count_;
};

int Return42() { return 42; }
int Negate(int a) { return -a; }
int Multiply(int a, int b) { return a * b; }

}  // namespace

// Try to catch any problem with scoped_refptr type deduction in rtc::Bind at
// compile time.
static_assert(is_same<detail::RemoveScopedPtrRef<
                          const scoped_refptr<RefCountInterface>&>::type,
                      scoped_refptr<RefCountInterface>>::value,
              "const scoped_refptr& should be captured by value");

static_assert(is_same<detail::RemoveScopedPtrRef<const scoped_refptr<F>&>::type,
                      scoped_refptr<F>>::value,
              "const scoped_refptr& should be captured by value");

static_assert(
    is_same<detail::RemoveScopedPtrRef<const int&>::type, const int&>::value,
    "const int& should be captured as const int&");

static_assert(
    is_same<detail::RemoveScopedPtrRef<const F&>::type, const F&>::value,
    "const F& should be captured as const F&");

static_assert(
    is_same<detail::RemoveScopedPtrRef<F&>::type, F&>::value,
    "F& should be captured as F&");

#define EXPECT_IS_CAPTURED_AS_PTR(T)                              \
  static_assert(is_same<detail::PointerType<T>::type, T*>::value, \
                "PointerType")
#define EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(T)                        \
  static_assert(                                                      \
      is_same<detail::PointerType<T>::type, scoped_refptr<T>>::value, \
      "PointerType")

EXPECT_IS_CAPTURED_AS_PTR(void);
EXPECT_IS_CAPTURED_AS_PTR(int);
EXPECT_IS_CAPTURED_AS_PTR(double);
EXPECT_IS_CAPTURED_AS_PTR(A);
EXPECT_IS_CAPTURED_AS_PTR(D);
EXPECT_IS_CAPTURED_AS_PTR(RefCountInterface*);

EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(RefCountInterface);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(B);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(C);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(E);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(F);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(RefCountedObject<RefCountInterface>);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(RefCountedObject<B>);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(RefCountedObject<C>);

TEST(BindTest, BindToMethod) {
  MethodBindTester object = {0};
  EXPECT_EQ(0, object.call_count);
  Bind(&MethodBindTester::NullaryVoid, &object)();
  EXPECT_EQ(1, object.call_count);
  EXPECT_EQ(1, Bind(&MethodBindTester::NullaryInt, &object)());
  EXPECT_EQ(2, object.call_count);
  EXPECT_EQ(2, Bind(&MethodBindTester::NullaryConst,
                    static_cast<const MethodBindTester*>(&object))());
  EXPECT_EQ(3, object.call_count);
  Bind(&MethodBindTester::UnaryVoid, &object, 5)();
  EXPECT_EQ(4, object.call_count);
  EXPECT_EQ(100, Bind(&MethodBindTester::Identity<int>, &object, 100)());
  EXPECT_EQ(5, object.call_count);
  const std::string string_value("test string");
  EXPECT_EQ(string_value, Bind(&MethodBindTester::Identity<std::string>,
                               &object, string_value)());
  EXPECT_EQ(6, object.call_count);
  int value = 11;
  EXPECT_EQ(12, Bind(&MethodBindTester::UnaryByRef, &object, value)());
  EXPECT_EQ(12, value);
  EXPECT_EQ(7, object.call_count);
  EXPECT_EQ(56, Bind(&MethodBindTester::Multiply, &object, 7, 8)());
  EXPECT_EQ(8, object.call_count);
}

TEST(BindTest, BindToFunction) {
  EXPECT_EQ(42, Bind(&Return42)());
  EXPECT_EQ(3, Bind(&Negate, -3)());
  EXPECT_EQ(56, Bind(&Multiply, 8, 7)());
}

// Test Bind where method object implements RefCountInterface and is passed as a
// pointer.
TEST(BindTest, CapturePointerAsScopedRefPtr) {
  LifeTimeCheck object;
  EXPECT_EQ(object.ref_count_, 0);
  scoped_refptr<LifeTimeCheck> scoped_object(&object);
  EXPECT_EQ(object.ref_count_, 1);
  {
    auto functor = Bind(&LifeTimeCheck::NullaryVoid, &object);
    EXPECT_EQ(object.ref_count_, 2);
    scoped_object = nullptr;
    EXPECT_EQ(object.ref_count_, 1);
  }
  EXPECT_EQ(object.ref_count_, 0);
}

// Test Bind where method object implements RefCountInterface and is passed as a
// scoped_refptr<>.
TEST(BindTest, CaptureScopedRefPtrAsScopedRefPtr) {
  LifeTimeCheck object;
  EXPECT_EQ(object.ref_count_, 0);
  scoped_refptr<LifeTimeCheck> scoped_object(&object);
  EXPECT_EQ(object.ref_count_, 1);
  {
    auto functor = Bind(&LifeTimeCheck::NullaryVoid, scoped_object);
    EXPECT_EQ(object.ref_count_, 2);
    scoped_object = nullptr;
    EXPECT_EQ(object.ref_count_, 1);
  }
  EXPECT_EQ(object.ref_count_, 0);
}

// Test Bind where method object is captured as scoped_refptr<> and the functor
// dies while there are references left.
TEST(BindTest, FunctorReleasesObjectOnDestruction) {
  LifeTimeCheck object;
  EXPECT_EQ(object.ref_count_, 0);
  scoped_refptr<LifeTimeCheck> scoped_object(&object);
  EXPECT_EQ(object.ref_count_, 1);
  Bind(&LifeTimeCheck::NullaryVoid, &object)();
  EXPECT_EQ(object.ref_count_, 1);
  scoped_object = nullptr;
  EXPECT_EQ(object.ref_count_, 0);
}

// Test Bind with scoped_refptr<> argument.
TEST(BindTest, ScopedRefPointerArgument) {
  LifeTimeCheck object;
  EXPECT_EQ(object.ref_count_, 0);
  scoped_refptr<LifeTimeCheck> scoped_object(&object);
  EXPECT_EQ(object.ref_count_, 1);
  {
    MethodBindTester bind_tester;
    auto functor =
        Bind(&MethodBindTester::RefArgument, &bind_tester, scoped_object);
    EXPECT_EQ(object.ref_count_, 2);
  }
  EXPECT_EQ(object.ref_count_, 1);
  scoped_object = nullptr;
  EXPECT_EQ(object.ref_count_, 0);
}

namespace {

const int* Ref(const int& a) { return &a; }

}  // anonymous namespace

// Test Bind with non-scoped_refptr<> reference argument.
TEST(BindTest, RefArgument) {
  const int x = 42;
  EXPECT_TRUE(Ref(x) == &x);
  // Bind() should not make a copy of |x|, i.e. the pointers should be the same.
  auto functor = Bind(&Ref, x);
  EXPECT_TRUE(functor() == &x);
}

}  // namespace rtc
#endif // UNIT_TEST
