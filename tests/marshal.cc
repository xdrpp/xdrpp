
#include "tests/xdrtest.hh"
#include <cassert>
#include <iostream>
#include <xdrpp/clear.h>
#include <xdrpp/depth_checker.h>
#include <xdrpp/marshal.h>
#include <xdrpp/printer.h>

using namespace std;

template<typename T>
typename std::enable_if<!xdr::xdr_traits<T>::has_fixed_size, std::size_t>::type
xdr_getsize(const T &t)
{
  return xdr::xdr_size(t);
}

template<typename T>
typename std::enable_if<xdr::xdr_traits<T>::has_fixed_size, std::size_t>::type
xdr_getsize(const T &t)
{
  assert(xdr::xdr_traits<T>::fixed_size == xdr::xdr_size(t));
  return xdr::xdr_traits<T>::fixed_size;
}

#define CHECK_SIZE(v, s)						\
do {									\
  size_t __s = xdr_getsize(v);						\
  if (__s != s) {							\
    cerr << #v << " has size " << __s << " should have " << s << endl;	\
    terminate();							\
  }									\
} while (0)


void
test_size()
{
  CHECK_SIZE(int32_t(), 4);
  CHECK_SIZE(fix_4(), 4);
  CHECK_SIZE(fix_12(), 12);
  CHECK_SIZE(xdr::opaque_array<5>(), 8);
  CHECK_SIZE(u_4_12(4), 8);
  CHECK_SIZE(u_4_12(12), 16);

  {
    bool ok = false;
    try { CHECK_SIZE(u_4_12(0), 9999); }
    catch (const xdr::xdr_bad_discriminant &) { ok = true; }
    assert(ok);
  }

  v12 v;
  CHECK_SIZE(v, 4);
  v.emplace_back();
  CHECK_SIZE(v, 16);
  v.emplace_back();
  CHECK_SIZE(v, 28);

  CHECK_SIZE(xdr::xstring<>(), 4);
  CHECK_SIZE(xdr::xstring<>("123"), 8);
  CHECK_SIZE((xdr::xvector<int32_t,5>()), 4);

  tuple<uint32_t, double> tn {99, 3.141592654};
  assert(xdr::xdr_traits<decltype(tn)>::has_fixed_size);
  CHECK_SIZE(tn, 12);

  tuple<xdr::xvector<int>, int, xdr::xstring<>> tv{{1, 2, 3}, 99, "hello"};
  assert(!xdr::xdr_traits<decltype(tv)>::has_fixed_size);
  CHECK_SIZE(tv, 32);
}

void
udsb(uint32_t, double, xdr::xstring<> &, bool, std::nullptr_t)
{
}

void
dump_indices(xdr::indices<>)
{
  cout << endl;
}
template<std::size_t N, std::size_t...Ns> void
dump_indices(xdr::indices<N, Ns...>)
{
  cout << " " << N;
  dump_indices(xdr::indices<Ns...>{});
}

//! Apply a function [object] to elements at a set of tuple indices,
//! with arbitrary arguments appended.
template<typename F, typename T, std::size_t...I, typename...A> inline auto
apply_indices(F &&f, T &&t, xdr::indices<I...>, A &&...a) ->
  decltype(f(std::get<I>(std::forward<T>(t))..., std::forward<A>(a)...))
{
  return f(std::get<I>(std::forward<T>(t))..., std::forward<A>(a)...);
}

void
test_tuple()
{
  using namespace xdr;
  using namespace detail;

  auto foo = make_tuple(uint32_t(6), 3.1415,
			xdr::xstring<>("Hello world"), true);
  decltype(foo) bar;
  xdr::xdr_from_msg(xdr_to_msg(foo), bar);
  apply_indices(udsb, foo, indices<0,1,2,3>{}, nullptr);
  apply_indices(udsb, foo, all_indices<4>{}, nullptr);
  assert (foo == bar);
}

// Test recursive structure for depth checking
struct TestNode
{
    int value;
    xdr::pointer<TestNode> left;
    xdr::pointer<TestNode> right;
};

// Test structure with multiple members of different depths
struct MultiDepthStruct
{
    xdr::xvector<xdr::xvector<xdr::xvector<int32_t>>> deep_member; // depth 3
    xdr::xvector<xdr::xvector<int32_t>> shallow_member;            // depth 2
};

namespace xdr
{
template <> struct xdr_traits<TestNode> : xdr_traits_base
{
    static Constexpr const bool is_class = true;
    static Constexpr const bool is_struct = true;
    static Constexpr const bool has_fixed_size = false;

    template <typename Archive>
    static void
    save(Archive& ar, const TestNode& obj)
    {
        archive(ar, obj.value, "value");
        archive(ar, obj.left, "left");
        archive(ar, obj.right, "right");
    }

    template <typename Archive>
    static void
    load(Archive& ar, TestNode& obj)
    {
        archive(ar, obj.value, "value");
        archive(ar, obj.left, "left");
        archive(ar, obj.right, "right");
    }

    static std::size_t
    serial_size(const TestNode& obj)
    {
        return xdr_traits<int>::serial_size(obj.value) +
               xdr_traits<pointer<TestNode>>::serial_size(obj.left) +
               xdr_traits<pointer<TestNode>>::serial_size(obj.right);
    }
};

template <> struct xdr_traits<MultiDepthStruct> : xdr_traits_base
{
    static Constexpr const bool is_class = true;
    static Constexpr const bool is_struct = true;
    static Constexpr const bool has_fixed_size = false;

    template <typename Archive>
    static void
    save(Archive& ar, const MultiDepthStruct& obj)
    {
        archive(ar, obj.deep_member, "deep_member");
        archive(ar, obj.shallow_member, "shallow_member");
    }

    template <typename Archive>
    static void
    load(Archive& ar, MultiDepthStruct& obj)
    {
        archive(ar, obj.deep_member, "deep_member");
        archive(ar, obj.shallow_member, "shallow_member");
    }

    static std::size_t
    serial_size(const MultiDepthStruct& obj)
    {
        return xdr_traits<decltype(obj.deep_member)>::serial_size(
                   obj.deep_member) +
               xdr_traits<decltype(obj.shallow_member)>::serial_size(
                   obj.shallow_member);
    }
};
}

xdr::pointer<TestNode>
create_test_tree(int depth, int value = 0)
{
    if (depth <= 0)
        return nullptr;

    auto node = xdr::pointer<TestNode>(new TestNode{value, nullptr, nullptr});
    node->left = create_test_tree(depth - 1, value * 2 + 1);
    node->right = create_test_tree(depth - 1, value * 2 + 2);
    return node;
}

void
test_depth_checker()
{

    // Test 1: Basic types should not count toward depth
    {
        int32_t simple = 42;
        assert(xdr::check_xdr_depth(simple, 0));

        xdr::xstring<> str = "Hello, world!";
        assert(xdr::check_xdr_depth(str, 0));

        xdr::opaque_array<10> arr;
        assert(xdr::check_xdr_depth(arr, 0));
    }

    // Test 2: Nested structures
    {
        // Understanding depth counting:
        // - A root TestNode with null children needs depth 2:
        //   - Depth 1: The TestNode itself (class)
        //   - Depth 2: Both pointer fields (containers) at same level
        {
            // Single node (no children)
            auto single =
                xdr::pointer<TestNode>(new TestNode{1, nullptr, nullptr});
            assert(!xdr::check_xdr_depth(*single, 0));
            assert(!xdr::check_xdr_depth(*single, 1));
            assert(xdr::check_xdr_depth(*single, 2));
        }

        {
            // Node with one child
            auto root =
                xdr::pointer<TestNode>(new TestNode{1, nullptr, nullptr});
            root->left =
                xdr::pointer<TestNode>(new TestNode{2, nullptr, nullptr});
            // Depth breakdown:
            // - Depth 1: Root TestNode
            // - Depth 2: Root's left pointer (which is non-null)
            // - Depth 3: Child TestNode
            // - Depth 4: Child's pointers
            assert(!xdr::check_xdr_depth(*root, 3));
            assert(xdr::check_xdr_depth(*root, 4));
        }

        {
            // Full binary tree of various depths
            for (int tree_depth = 1; tree_depth <= 3; ++tree_depth)
            {
                auto tree = create_test_tree(tree_depth);
                // For a binary tree of depth D:
                // - Each level adds 2 to the recursion depth (TestNode +
                // pointer level)
                // - So total depth = D * 2
                int expected_depth = tree_depth * 2;

                assert(!xdr::check_xdr_depth(*tree, expected_depth - 1));
                assert(xdr::check_xdr_depth(*tree, expected_depth));
            }
        }
    }

    // Test 3: nested containers
    {
        xdr::xvector<xdr::xvector<int32_t>> nested_vec;
        nested_vec.emplace_back();
        nested_vec[0].push_back(42);

        // Outer vector is depth 1, inner vector adds another level
        assert(!xdr::check_xdr_depth(nested_vec, 0));
        assert(!xdr::check_xdr_depth(nested_vec, 1));
        assert(xdr::check_xdr_depth(nested_vec, 2));
    }

    // Test 4: Mixed structures
    {
        // Depth should be 3
        // 1 for struct overhead + 2 for xvector<u_4_12> (1 for vector, 1 for
        // inner union type)
        testns::containertest ct;
        ct.uvec = {u_4_12(4), u_4_12(12)};
        ct.sarr[0] = "hello";
        ct.sarr[1] = "world";

        assert(!xdr::check_xdr_depth(ct, 0));
        assert(!xdr::check_xdr_depth(ct, 3));
        assert(xdr::check_xdr_depth(ct, 4));
    }

    // Test 5: Verify depth is max of members, not sum
    {
        MultiDepthStruct mds;

        // Initialize deep_member with depth 3 (vec<vec<vec<int>>>)
        mds.deep_member.emplace_back();
        mds.deep_member[0].emplace_back();
        mds.deep_member[0][0].push_back(42);

        // Initialize shallow_member with depth 2 (vec<vec<int>>)
        mds.shallow_member.emplace_back();
        mds.shallow_member[0].push_back(99);
        assert(!xdr::check_xdr_depth(mds, 3));
        assert(xdr::check_xdr_depth(mds, 4));
    }

    // Test 6: sanity check - verify depth_checker matches actual
    // marshal/unmarshal behavior
    {
        uint32_t original_limit = xdr::marshaling_stack_limit;

        auto test_type_depth = [](const auto& obj, const string& type_name) {
            for (uint32_t limit = 1; limit <= 10; ++limit)
            {
                xdr::marshaling_stack_limit = limit;

                bool depth_check_passes = xdr::check_xdr_depth(obj, limit);
                bool marshal_succeeds = true;
                try
                {
                    xdr::xdr_to_msg(obj);
                }
                catch (const xdr::xdr_stack_overflow& e)
                {
                    marshal_succeeds = false;
                    string expected_msg = "stack overflow in xdr_generic_put";
                    assert(string(e.what()) == expected_msg);
                }

                // Verify depth_checker matches actual behavior
                assert(depth_check_passes == marshal_succeeds);

                // Test unmarshaling if marshaling succeeded
                if (marshal_succeeds)
                {
                    try
                    {
                        auto msg = xdr::xdr_to_msg(obj);
                        typename std::remove_const<
                            typename std::remove_reference<
                                decltype(obj)>::type>::type copy;
                        xdr::xdr_from_msg(msg, copy);
                    }
                    catch (const xdr::xdr_stack_overflow&)
                    {
                        // This would be unexpected since marshaling succeeded
                        assert(false);
                    }
                }
            }
        };

        // Test 1: Binary trees of various depths
        for (int tree_depth = 1; tree_depth <= 3; ++tree_depth)
        {
            auto tree = create_test_tree(tree_depth);
            test_type_depth(*tree, "tree_depth_" + to_string(tree_depth));
        }

        // Test 2: Nested vectors
        {
            xdr::xvector<int32_t> vec1d;
            vec1d.push_back(42);
            test_type_depth(vec1d, "vector_1d");

            xdr::xvector<xdr::xvector<int32_t>> vec2d;
            vec2d.emplace_back();
            vec2d[0].push_back(42);
            test_type_depth(vec2d, "vector_2d");

            xdr::xvector<xdr::xvector<xdr::xvector<int32_t>>> vec3d;
            vec3d.emplace_back();
            vec3d[0].emplace_back();
            vec3d[0][0].push_back(42);
            test_type_depth(vec3d, "vector_3d");
        }

        // Test 3: MultiDepthStruct
        {
            MultiDepthStruct mds;
            mds.deep_member.emplace_back();
            mds.deep_member[0].emplace_back();
            mds.deep_member[0][0].push_back(42);
            mds.shallow_member.emplace_back();
            mds.shallow_member[0].push_back(99);
            test_type_depth(mds, "MultiDepthStruct");
        }

        // Test 4: Arrays
        {
            xdr::xarray<int32_t, 5> arr;
            arr.fill(42);
            test_type_depth(arr, "array_int32");

            xdr::xarray<xdr::xvector<int32_t>, 3> arr_vec;
            for (auto& v : arr_vec)
            {
                v.push_back(42);
            }
            test_type_depth(arr_vec, "array_of_vectors");
        }

        // Test 5: Pointers
        {
            xdr::pointer<int32_t> null_ptr;
            test_type_depth(null_ptr, "null_pointer");

            xdr::pointer<int32_t> int_ptr(new int32_t(42));
            test_type_depth(int_ptr, "int_pointer");

            xdr::pointer<xdr::xvector<int32_t>> vec_ptr(
                new xdr::xvector<int32_t>());
            vec_ptr->push_back(42);
            test_type_depth(vec_ptr, "vector_pointer");
        }

        // Test 6: Test types from xdrtest
        {
            testns::containertest ct;
            ct.uvec = {u_4_12(4), u_4_12(12)};
            ct.sarr[0] = "hello";
            ct.sarr[1] = "world";
            test_type_depth(ct, "containertest");

            testns::numerics n;
            n.b = true;
            n.i1 = 0x12345678;
            n.i2 = 0xffffffff;
            n.i3 = UINT64_C(0x123456789abcdef0);
            n.i4 = UINT64_C(0xfedcba9876543210);
            n.f1 = 3.14159f;
            n.f2 = 2.71828;
            n.e1 = testns::RED;
            test_type_depth(n, "numerics");

            testns::uniontest u;
            u.ip.activate() = 0x12349876;
            u.key.arbitrary(::REDDEST); // Use global color enum value
            u.key.big() = {5, 4, 3, 2, 1};
            test_type_depth(u, "uniontest");
        }

        // Restore original limit so other tests aren't affected
        xdr::marshaling_stack_limit = original_limit;
    }
}

int
main()
{
  test_size();
  test_tuple();
  test_depth_checker();

  testns::bytes b1, b2;
  xdr::xdr_clear(b2);
  b1.s = "Hello world\n";
  b1.fixed.fill(0xc5);
  b1.variable = { 2, 4, 6, 8 };
  
  xdr::xdr_from_msg(xdr::xdr_to_msg(b1), b2);
  assert(b1.s == b2.s);
  assert(b1.fixed == b2.fixed);
  assert(b1.variable == b2.variable);

  xdr::xdr_from_opaque(xdr::xdr_to_opaque(b1), b2);
  assert(b1.s == b2.s);
  assert(b1.fixed == b2.fixed);
  assert(b1.variable == b2.variable);

  testns::numerics n1, n2;
  xdr::xdr_clear(n2);
  n1.b = false;
  n2.b = true;
  n1.i1 = 0x7eeeeeee;
  n1.i2 = 0xffffffff;
  n1.i3 = UINT64_C(0x7ddddddddddddddd);
  n1.i4 = UINT64_C(0xfccccccccccccccc);
  n1.f1 = 3.141592654;
  n1.f2 = 2.71828182846;
  n1.e1 = testns::REDDER;
  n2.e1 = testns::REDDEST;

  {
    unique_ptr<double> dp1 (new double (3.141592653));
    uint64_t x = xdr::xdr_reinterpret<uint64_t>(*dp1);
    assert (!memcmp(&x, dp1.get(), sizeof(x)));
    x = xdr::xdr_traits<double>::to_uint(*dp1);
    assert (memcmp(&x, dp1.get(), sizeof(x)) == 0);
    unique_ptr<double> dp2 (new double (1.23456789));
    *dp2 = xdr::xdr_traits<double>::from_uint(x);
    assert (!memcmp(&x, dp2.get(), sizeof(x)));
    assert (*dp1 == *dp2);
  }

  xdr::xdr_from_msg(xdr::xdr_to_msg(n1), n2);
  assert(xdr::xdr_to_string(n1) == xdr::xdr_to_string(n2));
  assert(n1.b == n2.b);
  assert(n1.i1 == n2.i1);
  assert(n1.i2 == n2.i2);
  assert(n1.i3 == n2.i3);
  assert(n1.i4 == n2.i4);
  assert(n1.f1 == n2.f1);
  assert(n1.f2 == n2.f2);
  assert(n1.e1 == n2.e1);

  testns::uniontest u1, u2;
  xdr::xdr_clear(u2);
  u1.ip.activate() = 0x12349876;
  u1.key.arbitrary(REDDEST);
  u1.key.big() = { 5, 4, 3, 2, 1, 0, 0, 0, 255 };

  {
    xdr::msg_ptr m (xdr::xdr_to_msg(u1));
    xdr::xdr_from_msg(m, u2);
    assert(xdr::xdr_to_string(u1) == xdr::xdr_to_string(u2));
  }

  assert(*u1.ip == *u2.ip);
  assert(u1.key.arbitrary() == u2.key.arbitrary());
  assert(u1.key.big() == u2.key.big());
  {
    bool ok = false;
    try { u2.key.medium() = 7777; }
    catch (const xdr::xdr_wrong_union &) { ok = true; }
    assert (ok);
  }

  {
    testns::unionvoidtest uv;
    uv.arbitrary(RED);
    assert(uv.arbitrary() == RED);
    uv.arbitrary(REDDER);
    assert(uv.arbitrary() == REDDER);
    uv.arbitrary(REDDEST);
    assert(uv.arbitrary() == REDDEST);
  }

  {
    testns::containertest ct1, ct2;
    xdr::xdr_clear(ct2);

    ct1.uvec = { u_4_12(4), u_4_12(12), u_4_12(4), u_4_12(4) };
    ct1.sarr[0] = "hello";
    ct1.sarr[1] = "world";

    xdr::msg_ptr m (xdr::xdr_to_msg(ct1));
    xdr::xdr_from_msg(m, ct2);
    assert(xdr::xdr_to_string(ct1) == xdr::xdr_to_string(ct2));

    assert(ct1.uvec.at(0).f4().i == ct2.uvec.at(0).f4().i);
    assert(ct1.uvec.at(1).f12().i == ct2.uvec.at(1).f12().i);
    assert(ct1.uvec.at(1).f12().d == ct2.uvec.at(1).f12().d);
    assert(ct1.uvec.at(2).f4().i == ct2.uvec.at(2).f4().i);
    assert(ct1.uvec.at(3).f4().i == ct2.uvec.at(3).f4().i);
    for (unsigned i = 0; i < 2; i++)
      assert(ct1.sarr.at(i) == ct2.sarr.at(i));

    testns::containertest1 ct3;
    bool ok = false;
    try { xdr::xdr_from_msg(m, ct3); }
    catch (const xdr::xdr_overflow &) { ok = true; }
    assert(ok);
  }

  return 0;
}
