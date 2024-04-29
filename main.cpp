#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) 
{
    return path(data, data + sz);
}

// напишите эту функцию
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    static regex include_file /* #include "..." */ (R"/(\s*#\s*include\s*"([^ "]*)"\s*)/");
    static regex include_lib /* #include <...> */ (R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    smatch m;
    string text;
    size_t line = 0;
    ifstream in(in_file);
    ofstream out(out_file, ios::out | ios::app);

    while (getline(in, text)) 
    {
        ++line;
        if (regex_match(text, m, include_file)) 
        {
            path p = string(m[1]);
            // Получите папку текущего файла через метод parent_path
            path path = in_file.parent_path() / p;
            // Полезно использовать метод is_open класса ifstream, чтобы проверить, был ли открыт файл. 
            ifstream ifstr;
            ifstr.open(path);

            if (ifstr.is_open())
            {
                /* Рекурсивная функция (поток ввода, поток вывода, 
                имя файла, к которому относится поток ввода, чтобы в случае чего выдать ошибку,
                вектор include-директорий) */
                Preprocess(path, out_file, include_directories);
                ifstr.close();
            }

            else 
            {
            bool found = false;
            // Если файл не найден, поиск выполняется последовательно по всем элементам вектора include_directories
            for (const auto& file : include_directories) 
            {
                using filesystem::path;
                if (exists(path(file / p))) 
                {
                    found = true;

                    Preprocess(file / p, out_file, include_directories);
                    break;
                }
            }

            if (!found) 
            {
                /* Если включаемый файл так и не был найден или его не удалось открыть через ifstream, функция должна вывести в cout следующий текст: */
                cout << "unknown include file "s << p.filename().string() << " at file "s << in_file.string() << " at line "s << line << endl;
            return found;
            }
            }
        }

        else if (regex_match(text, m, include_lib)) 
        {
            path p = string(m[1]);

            bool found = false;
            // Если файл не найден, поиск выполняется последовательно по всем элементам вектора include_directories
            for (const auto& file : include_directories) 
            {
                if (exists(path(file / p))) 
                {
                    found = true;

                    Preprocess(file / p, out_file, include_directories);
                    break;
                }
            }

            if (!found) 
            {
                /* Если включаемый файл так и не был найден или его не удалось открыть через ifstream, функция должна вывести в cout следующий текст: */
                cout << "unknown include file "s << p.filename().string() << " at file "s << in_file.string() << " at line "s << line << endl;
            return found;
            }
            
        }

        else 
        {
            out << text << endl;
        }
    }
    
    return true;
}

string GetFileContents(string file) 
{
    ifstream stream(file);

    // конструируем string по двум итераторам
    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

void Test() 
{
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include \"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#   include<dummy.txt>\n"
            "}\n"sv;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include"sv;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "// text from c.h after include\n"sv;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"sv;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"sv;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"sv;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

    ostringstream test_out;
    test_out << "// this comment before include\n"
        "// text from b.h before include\n"
        "// text from c.h before include\n"
        "// std1\n"
        "// text from c.h after include\n"
        "// text from b.h after include\n"
        "// text between b.h and c.h\n"
        "// text from d.h before include\n"
        "// std2\n"
        "// text from d.h after include\n"
        "\n"
        "int SayHello() {\n"
        "    cout << \"hello, world!\" << endl;\n"sv;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() 
{
    Test();
}