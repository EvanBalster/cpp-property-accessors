# Property Accessors for C++
*Developed in a fugue state by Evan Balster, January 2025*

This is a header-only library implementing zero-overhead **property accessors** — that is, synthetic variables which are actually provided by getter and/or setter functions.

Property accessors are commonly used to make members of one class look like members of another class that refers to it, or to provide multiple mutable representations of the same information.  This syntactic sugar is a popular feature of other high-level languages such as C# but sadly hasn't been accepted into standard C++.

<span style="background-color: yellow;">**The API is a work-in-progress and subject to change.**</span>

### Features

* **Zero memory overhead**.
* **Zero runtime overhead** when compiled with optimization.
* **Proxy property accessors** based on reference expressions, for concealing indirection.
* **Value property accessors** based on "get" and/or "set" expressions, for alternative representations of data.
* **Property member access** via `->`, or via `.` member access using a `mimic` specialization.
* **Full operator support**, behaving as if the property accessor were replaced by value.
* **Full const-correctness support**.

### Example

Suppose we want a nicer interface to this bare-bones data structure:

```c++
struct Rect
{
    int x1, x2, y1, y2;
    
    int area() const    {return (x2-x1) * (y2-y1);}
};
```

Using this library we can create a manipulator class called **Virtual_Rect** which provides read-write access to a rectangle's corner coordinates as well as its `width` and `height`, and a read-only `area` property.  Its memory layout will consist of just a single pointer `Rect*`, but these properties will act like real member variables.

Property accessors are declared in a "block", together with the underlying variables.  This block is in fact a union where every data member has the same layout.  We can use the handy `PropertyAccessors` macro to hide away the details:

```c++
#include <property_accessor.h>

struct Virtual_Rect
{
    struct RectPtr {Rect *rect;};

    PropertyAccessors(RectPtr,
                      
        // Give the real RectPtr structure a name so we can assign to it.
        UnionMember(RectPtr rect_ptr;),
    
        // Proxy (reference) property accessors
        Proxy  (int,  x1,     rect->x1),
        Proxy  (int,  x2,     rect->x2),
        Proxy  (int,  y1,     rect->y1),
        Proxy  (int,  y2,     rect->y2),
                    
        // Value (get/set by copy) property accessors
        GetSet (int,  width,  rect->x2 - rect->x1,
                int new_width,           rect->x2 = rect->x1 + new_width),
        GetSet (int,  height, rect->y2 - rect->y1,
                int new_height,          rect->y2 = rect->y1 + new_height),
        GetOnly(int,  area,   rect->area())

   );
};
```

<blockquote>

<details> <summary>🔎 <strong>See how to declare property accessors without macros</strong>, for more control.</summary>

Under the hood, property accessors are based on getter/setter types.  Each of these inherits from the 'actual' data type, in this case `RectPtr`.  Proxy accessors only need a get() function returning a reference.  Value accessors use a get() function and optionally also a set() function.  Finally, we declare a union with property accessors using each of our getter/setter types.

Compared to using the macro, this approach provides more control over `const` semantics.  For example, we could choose to add a `const` qualifier to the `set` methods for accessing `width` and `height`.  This would allow 

```c++
#include <property_accessor_nomacros.h>

struct Virtual_Rect
{
    // The real data underlying the property accessor group.
    struct RectPtr {Rect *rect;};
    
    // Define reference-getters for proxy property accessors.
    struct acc_x1 : RectPtr {int& get() const {return rect->x1;}};
    struct acc_x2 : RectPtr {int& get() const {return rect->x2;}};
    struct acc_y1 : RectPtr {int& get() const {return rect->y1;}};
    struct acc_y2 : RectPtr {int& get() const {return rect->y2;}};

    // Define getter/setters for value property accessors.
    struct acc_width : RectPtr
    {
        int  get() const         {return rect->x2 - rect->x1;}
        void set(int new_width)  {rect->x2 = rect->x1 + new_width;}
    };
    struct acc_height : RectPtr
    {
        int  get() const         {return (rect->y2 - rect->y1);}
        void set(int new_height) {rect->y2 = rect->y1 + new_height;}
    };
    struct acc_area : RectPtr {int get() const {return rect->area();}};
    
    // Property accessors 
    union
    {
        // Give the real RectPtr structure a name so we can assign to it.
        RectPtr                       rect_ptr;
        
        // Proxy (reference) property accessors
        property_accessor<acc_x1>     x1;
        property_accessor<acc_x2>     x2;
        property_accessor<acc_y1>     y1;
        property_accessor<acc_y2>     y2;
        
        // Value (get/set by copy) property accessors
        property_accessor<acc_width>  width;
        property_accessor<acc_height> height;
        property_accessor<acc_area>   area;
    };
};
```

As a middle ground, it's also possible to use the `Custom` option in the `PropertyAccessors` macro.  This allows you to define your own `get` and `set` functions and handles the rest automatically.

</details>

</blockquote>

The properties of the resulting class act like regular member variables of type `int`.  Math operators and other operator-based logic will work automatically:

```c++
void PrintVRect(const Virtual_Rect &vrect)
{
    // Properties support iostreams and acting as right-hand operands in general.
    std::cout << "VRect: {"
            << vrect.x1 << "," << vrect.y1 << ", "
            << vrect.x2 << "," << vrect.y2 << "} "
        << vrect.width << "x" << vrect.height << " = " << vrect.area
        << " ... width+height = " << (vrect.width+vrect.height) << std::endl;
}

int main(int argc, char **argv)
{
    Rect rect = {0,2,  1,4};

    Virtual_Rect vrect = {&rect};

    PrintVRect(vrect);

    // We can manipulate width and height
    vrect.width += 2;
    vrect.height += 3;
    
    // We can't manipulate area because it's a get-only property.
    //vrect.area = 7;  <-- this line won't compile!

    PrintVRect(vrect);
}
```

Property accessors could also be added to the `Rect` object directly, giving it a richer function.  However, any variables used by property accessors need to exist in the underlying union.

It would be similarly possible to put these properties directly into `Rect`, except we would need to place its member variables inside the union with the properties that access them.

### Feature: Property Member Access

If a property accessor refers to a class or struct type, any member variables or methods of that class must be accessed with `->`.  We can overcome this limitation with a specialization called a "mimic", which provides dot `.` access to listed members.

This specialization must be placed in the global namespace, and must be visible to any code declaring a property accessor using the type in question.  When possible, it's best to place our mimic declaration right after the type in question.

```c++
#include <property_accessor.h>

struct Rect
{
    int x1, x2, y1, y2;
    
    int area() const    {return (x2-x1) * (y2-y1);}
};

// Make Rect-type property accessors work more like the real thing.
PropertyAccess_Mimic(Rect,
    Variables(x1, y1, x2, y2),
    Methods(area));
```

<blockquote>

<details> <summary>🔎<strong>See how to do this without macros</strong>.</summary>


We can specialize the mimic template ourselves.  The `property_access::member` template handles access to member variables.  When forwarding member functions, we use `_property_getset.get()` which returns either a value or reference depending on whether the specialization is applied to a proxy property or a value property.

```c++
#include <property_accessor_nomacros.h>

// Make Rect-type properties work more like the real thing.
template<typename GetSet_t> struct property_access::mimic<Rect, GetSet_t>
{
    union
    {
        // This variable is required in any specialization of property_access::mimic.
        GetSet_t _property_getset;

        // These accessors automatically extend proxy or value access to Rect's members.
        property_access::member<GetSet_t, int, &Rect::x1> x1;
        property_access::member<GetSet_t, int, &Rect::y1> y1;
        property_access::member<GetSet_t, int, &Rect::x2> x2;
        property_access::member<GetSet_t, int, &Rect::y2> y2;
    };
    
    // Forward the area() method.
    int area() const    {return _property_getset.get().area();}
};
```

</details>

</blockquote>

We can even change mimicked variables on a get-set property.  In this case, a temporary copy of the property value will be made, its member variable will be changed, and the setter will be called with the modified property value.

### Feature: Proxy Accessors

Proxy property accessors are based on a getter function that returns an lvalue reference to some value or object.  This reference may be const.

These are useful for making something look like a member of your class when it's actually part of another object.  For example, the variables of an entity may be made to appear as variables of all its components.

Proxy accessors are both reference-like and pointer-like.  They may be implicitly converted to a reference or pointer.  They support dereferencing with `*` and accessing members of the referent with `->`.

### Feature: Value Accessors

Value property accessors are based on a getter function (returning a non-reference value) and a setter function.  Copies of the property's value are made whenever it is accessed.  If the value is settable, it supports compound assignments and incrementations as well as modification of mimicked members by making a temporary copy behind the scenes.

These are useful for exposing multiple representations of data — for example, representing an angle in both degrees and radians.  They can be used to implement encapsulation of private variables while leaving client

Value accessors can be implicitly cast to their value type.  They support some limited pointer semantics for consistency with proxy accessors.  They can be dereferenced (to a value) using the `*` operator.  The `->` operator can also be used (it works by creating a temporary copy of the value).  All of these functions

### Operator Overloading

Supported operators may be used on any property referring to a value that supports those operators.  The result will be as if the property's result

These operators are supported by all property accessors:

​    `() []  + - * / %  << >>  == != > >= < <=  ! ~ | & ^`

* **Proxy** accessors forward these operators to the referenced object.
* **Value** accessors apply these operators to the result of their `get` function.  <span style="background-color: yellow;">`set` is not invoked even if the value was changed</span>.

These operators are supported by proxy property accessors and settable value property accessors:

​    `=  += -= *= /= %=  <<= >>=  &= |= ^=  ++ --`

* **Proxy** accessors forward any assignment or increment operators to the referenced object.

* **Value** accessors make a temporary value, assign or increment it, and invoke `set` with the modified value.

The following operators are not forwarded to destination values, for the reasons stated.

* `& ->*` — <span style="background-color: yellow;">Support for forwarding these operators may be added in the future as use-cases become clearer.</span>
* `* ->` — Used to dereference properties and access their values.  <span style="background-color: yellow;">Support may be added if use-cases become clearer.</span>
* `, && ||` — Rarely-used operators with a high likelihood of creating logic errors.  Support is not planned.
* `new delete new[] delete[]` — Property accessors should never be independently allocated or deleted.

### Const Correctness

Property accessors will preserve the `const` semantics of the getters and setters used to define them.

The `PropertyAccessors` macro declares all `get` functions const and all `set` functions non-const.  To make a settable property behave like a `mutable` member, you'll need to define its property 

There is some room for improvement with regard to const-correctness.  The predecessor to this library only supported proxy property accessors, so there is a tendency toward assuming const qualification.

<span style="background-color: yellow;">In the case of value property accessors, operators other than assignments, compound assignments and increments will not invoke `set`.</span>
