struct Foo {
    enum class Encoding {
        V1,
        V2, //!< BIP155 encoding
    };
    enum class Type {
        Network,
        Disk
    };
    struct SerParams {
        const Encoding enc;
        const Type type;
    };
};

void func()
{
    //Foo::SerParams foo; // compile error
    Foo::SerParams a{}; //bad
    Foo::SerParams b{{}}; //bad
    Foo::SerParams c{Foo::Encoding::V1}; //bad
    Foo::SerParams q{{},Foo::Type::Network}; //bad
    Foo::SerParams q2{{},{}}; //bad
    Foo::SerParams d({}); //bad
    Foo::SerParams e({Foo::Encoding::V1}); //bad

    Foo::SerParams f({Foo::Encoding::V1, Foo::Type::Network}); //ok
    Foo::SerParams g{Foo::Encoding::V1, Foo::Type::Network}; //ok
}
