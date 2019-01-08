//
//  HelloWorld.cpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/12/7.
//  Copyright © 2018 Binfeng. All rights reserved.
//

#include <iostream>
#include <any>
#include <iterator>
#include <algorithm>
#include <list>
#include <variant>
#include <optional>
#include <vector>
#include <complex>

using namespace std;

int main_hello(){
    int N[] = {0,0,0};
    
    cout <<"std::numeric_limits<long int>::digits:" <<std::numeric_limits<long int>::digits <<endl;
    cout << "std::numeric_limits<int>::digits:"<<std::numeric_limits<int>::digits << endl;
    cout << "std::numeric_limits<unsigned int>::digits:" << std::numeric_limits<unsigned int>::digits << endl;
    cout << "std::numeric_limits<short>::digits:" << std::numeric_limits<short>::digits << endl;
    cout << "std::numeric_limits<unsigned short>::digits:" << std::numeric_limits<unsigned short>::digits << endl;
    if(std::numeric_limits<long int>::digits == 63 &&
       std::numeric_limits<int>::digits ==31 &&
       std::numeric_limits<unsigned int>::digits == 32){
        long int i= -0xffffffff;
        cout << i << endl;
        for(long int i= -0xffffffff; i; --i){
            cout << i << endl;
            N[i] = 1;
        }
        cout <<"here" << endl;
    }else{
        cout << "there" << endl;
        N[1] =1;
    }
    cout << N[0] << N[1] << N[2] << endl;
    
    std::list<any> values;
    any to_append = 100;
    values.push_back(to_append);
    values.push_back(12.0);
    values.push_back("this is any");
    
    std::cout << "#empty == "
    << std::count_if(values.begin(), values.end(),
                     [](const any & operand)->bool {
                         return operand.has_value();
                     })
    << std::endl;
    
    variant<int, float,unique_ptr<string>> myvariant;
    myvariant = 10;
    cout <<"myvir:0:" <<std::get<0>(myvariant) << endl;
    myvariant = 10.333f;
    cout <<"myvir:double:" <<std::get<1>(myvariant) << endl;
    myvariant = std::make_unique<string>("test variant") ;
    cout << std::get<2>(myvariant)->length() << endl;
    
    if (const auto intPtr (std::get_if<int>(&myvariant)); intPtr)
        std::cout << "int!" << *intPtr << "\n";
    else if (const auto floatPtr (std::get_if<float>(&myvariant)); floatPtr)
        std::cout << "float!" << *floatPtr << "\n";
    
    if (std::holds_alternative<int>(myvariant))
        std::cout << "the variant holds an int!\n";
    else if (std::holds_alternative<float>(myvariant))
        std::cout << "the variant holds a float\n";
    else if (std::holds_alternative<unique_ptr<std::string>>(myvariant))
        std::cout << "the variant holds a string\n";
    
    std::optional<int> oEmpty;
    std::optional<float> oFloat = std::nullopt;
    // direct:
    std::optional<int> oInt(10);
    
    // make_optional
    auto oDouble = std::make_optional(3.0);
    auto oComplex = make_optional<std::complex<double>>(3.0, 4.0);
    // in_place
    std::optional<std::complex<double>> o7{std::in_place, 3.0, 4.0};
    // will call vector with direct init of {1, 2, 3}
    std::optional<std::vector<int>> oVec(std::in_place, {1, 2, 3});
    // copy/assign:
    auto oIntCopy = oInt;
    
    auto next = [](auto n)->std::variant<int, double>{ return n + 1; };
    auto d = std::visit(next, std::variant<int, double>(3.14));
    assert( std::get<double>(d) = 4.14 );
    
    char c = 0b010101;
    auto add = [](auto a,auto b){return a + b;};
    cout << add(10,9) << endl;
    cout << add(10.2,4.55) <<endl;
    
    
    
    return 0;
}
