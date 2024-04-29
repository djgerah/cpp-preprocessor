#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

// using namespace std;
// using filesystem::path;
using namespace std::string_literals;

std::filesystem::path operator""_p(const char* data, std::size_t sz)
{
    return std::filesystem::path(data, data + sz);
}

bool Preprocess(const std::filesystem::path& in_file, const std::filesystem::path& out_file, const std::vector<std::filesystem::path>& include_directories);

bool RecursivePreprocess(const std::filesystem::path& in_file, const std::filesystem::path& out_file, std::filesystem::path file, size_t line, const std::vector<std::filesystem::path>& include_directories)
{
    bool found = false;
    // Если файл не найден, поиск выполняется последовательно по всем элементам вектора include_directories
    for (const auto& path : include_directories)
    {
        if (exists(std::filesystem::path(path / file)))
        {
            found = true;
            Preprocess(path / file, out_file, include_directories);
        }
    }

    if (!found)
    {
        // Если включаемый файл так и не был найден или его не удалось открыть через ifstream, 
        // функция должна вывести в cout следующий текст:
        std::cout << "unknown include file "s << file.filename().string() << " at file "s << in_file.string() << " at line "s << line << std::endl;
    }
    return found;
}

bool Preprocess(const std::filesystem::path& in_file, const std::filesystem::path& out_file, const std::vector<std::filesystem::path>& include_directories)
{
    static std::regex include_file (R"/(\s*#\s*include\s*"([^ "]*)"\s*)/"); // #include "..."
    static std::regex include_lib (R"/(\s*#\s*include\s*<([^>]*)>\s*)/"); // #include <...>
    std::smatch m;
    std::string text;
    size_t line = 0;
    std::ifstream in;
    if(!in.is_open()) in.open(in_file);
    std::ofstream out;
    if(!out.is_open()) out.open(out_file, std::ios::out | std::ios::app);

    while (getline(in, text))
    {
        ++line;
        if (regex_match(text, m, include_file))
        {
            std::filesystem::path name_file = std::string(m[1]);
            // Получите папку текущего файла через метод parent_path
            std::filesystem::path path = in_file.parent_path() / name_file;
            // Полезно использовать метод is_open класса ifstream, чтобы проверить, был ли открыт файл. 
            std::ifstream file;
            file.open(path);

            if (file.is_open())
            {
                Preprocess(path, out_file, include_directories);
                file.close();
            }

            else
            {
                /* Рекурсивная функция (поток ввода, поток вывода,
                имя файла, к которому относится поток ввода, чтобы в случае чего выдать ошибку, номер строки,
                вектор include-директорий) */
                bool found = RecursivePreprocess(in_file, out_file, name_file, line, include_directories);
                
                if (!found)
                {
                    return false;
                }
            }
        }

        else if (regex_match(text, m, include_lib))
        {
            std::filesystem::path name_file = std::string(m[1]);

            /* Рекурсивная функция (поток ввода, поток вывода,
            имя файла, к которому относится поток ввода, чтобы в случае чего выдать ошибку, номер строки,
            вектор include-директорий) */
            bool found = RecursivePreprocess(in_file, out_file, name_file, line, include_directories);
            
            if (!found)
            {
                return false;
            }

        }

        else
        {
            out << text << std::endl;
        }
    }

    return true;
}

std::string GetFileContents(std::string file)
{
    std::ifstream stream(file);

    // конструируем string по двум итераторам
    return { (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>() };
}

void Test()
{
    using namespace std;
    
    std::error_code err;
    std::filesystem::remove_all("sources"_p, err);
    std::filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    std::filesystem::create_directories("sources"_p / "include1"_p, err);
    std::filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

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

    return 0;
}