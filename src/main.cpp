#include <wayfire/plugin.hpp>
#include <wayfire/core.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>

namespace
{

std::string trim(const std::string& str)
{
    const auto first = str.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
    {
        return {};
    }

    const auto last = str.find_last_not_of(" \t\r\n");

    return str.substr(first, last - first + 1);
}

std::string find_config_file()
{
    auto& core = wf::get_core();

    for (int i = 0; i < core.argc; ++i)
    {
        const std::string arg = core.argv[i];

        if ((arg == "-c" || arg == "--config") && i + 1 < core.argc)
        {
            return core.argv[i + 1];
        }

        if (arg.rfind("--config=", 0) == 0)
        {
            return arg.substr(9);
        }
    }

    const char* home = std::getenv("HOME");

    if (!home)
    {
        return {};
    }

    const std::string config1 =
        std::string(home) + "/.config/wayfire.ini";

    if (std::filesystem::exists(config1))
    {
        return config1;
    }

    const std::string config2 =
        std::string(home) + "/.config/wayfire/wayfire.ini";

    if (std::filesystem::exists(config2))
    {
        return config2;
    }

    return {};
}

bool valid_name(const std::string& name)
{
    if (name.empty())
    {
        return false;
    }

    for (std::size_t i = 0; i < name.size(); ++i)
    {
        const char c = name[i];

        const bool alpha =
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z');

        const bool digit =
            (c >= '0' && c <= '9');

        if (alpha || digit || c == '_')
        {
            if (i == 0 && digit)
            {
                return false;
            }

            continue;
        }

        return false;
    }

    return true;
}

void run_process(
    const std::string& program,
    const std::vector<std::string>& arguments)
{
    std::vector<char*> argv;

    argv.push_back(const_cast<char*>(program.c_str()));

    for (const auto& argument : arguments)
    {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }

    argv.push_back(nullptr);

    const pid_t pid = fork();

    if (pid == 0)
    {
        execvp(program.c_str(), argv.data());
        _exit(127);
    }

    if (pid > 0)
    {
        waitpid(pid, nullptr, 0);
    }
}

void update_activation_environment(
    const std::vector<std::string>& variables)
{
    std::vector<std::string> assignments;

    for (const auto& name : variables)
    {
        const char* value = std::getenv(name.c_str());

        if (!value)
        {
            continue;
        }

        assignments.push_back(
            name + "=" + std::string(value));
    }

    if (assignments.empty())
    {
        return;
    }

    run_process(
        "systemctl",
        [&]()
        {
            std::vector<std::string> args;

            args.push_back("--user");
            args.push_back("import-environment");

            for (const auto& assignment : assignments)
            {
                args.push_back(assignment);
            }

            return args;
        }());

    run_process(
        "dbus-update-activation-environment",
        [&]()
        {
            std::vector<std::string> args;

            args.push_back("--systemd");

            for (const auto& assignment : assignments)
            {
                args.push_back(assignment);
            }

            return args;
        }());
}

bool is_environment_first(
    const std::vector<std::string>& plugins)
{
    return !plugins.empty() &&
        plugins.front() == "environment";
}

bool read_core_plugins(
    const std::string& config_file,
    std::vector<std::string>& plugins,
    std::size_t& plugins_line)
{
    std::ifstream file(config_file);

    if (!file.is_open())
    {
        return false;
    }

    bool in_core = false;
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(file, line))
    {
        ++line_number;

        const std::string trimmed = trim(line);

        if (trimmed.empty())
        {
            continue;
        }

        if (trimmed[0] == '#' || trimmed[0] == ';')
        {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']')
        {
            const std::string section =
                trim(trimmed.substr(1, trimmed.size() - 2));

            in_core = (section == "core");

            continue;
        }

        if (!in_core)
        {
            continue;
        }

        const auto equal = trimmed.find('=');

        if (equal == std::string::npos)
        {
            continue;
        }

        const std::string key =
            trim(trimmed.substr(0, equal));

        if (key != "plugins")
        {
            continue;
        }

        std::string value =
            trim(trimmed.substr(equal + 1));

        std::size_t start = 0;

        while (start < value.size())
        {
            while (start < value.size() &&
                   std::isspace(
                       static_cast<unsigned char>(value[start])))
            {
                ++start;
            }

            if (start >= value.size())
            {
                break;
            }

            std::size_t end = start;

            while (end < value.size() &&
                   !std::isspace(
                       static_cast<unsigned char>(value[end])))
            {
                ++end;
            }

            plugins.push_back(
                value.substr(start, end - start));

            start = end;
        }

        plugins_line = line_number - 1;

        return true;
    }

    return false;
}

bool rewrite_plugins(
    const std::string& config_file,
    const std::vector<std::string>& plugins,
    const std::size_t plugins_line)
{
    std::ifstream input(config_file);

    if (!input.is_open())
    {
        return false;
    }

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(input, line))
    {
        lines.push_back(line);
    }

    if (plugins_line >= lines.size())
    {
        return false;
    }

    std::string indentation;

    for (const char c : lines[plugins_line])
    {
        if (c == ' ' || c == '\t')
        {
            indentation += c;
        }
        else
        {
            break;
        }
    }

    std::string plugin_line =
        indentation + "plugins =";

    for (const auto& plugin : plugins)
    {
        plugin_line += " ";
        plugin_line += plugin;
    }

    lines[plugins_line] = plugin_line;

    const std::string temporary_file =
        config_file + ".environment.tmp";

    {
        std::ofstream output(
            temporary_file,
            std::ios::trunc);

        if (!output.is_open())
        {
            return false;
        }

        for (const auto& current_line : lines)
        {
            output << current_line << '\n';
        }

        output.flush();

        if (!output)
        {
            return false;
        }
    }

    if (std::rename(
            temporary_file.c_str(),
            config_file.c_str()) != 0)
    {
        std::remove(temporary_file.c_str());
        return false;
    }

    return true;
}

bool ensure_environment_first()
{
    const std::string config_file =
        find_config_file();

    if (config_file.empty())
    {
        return false;
    }

    std::vector<std::string> plugins;
    std::size_t plugins_line = 0;

    if (!read_core_plugins(
            config_file,
            plugins,
            plugins_line))
    {
        return false;
    }

    bool found = false;

    std::vector<std::string> reordered;

    for (const auto& plugin : plugins)
    {
        if (plugin == "environment")
        {
            found = true;
            continue;
        }

        reordered.push_back(plugin);
    }

    if (!found)
    {
        return false;
    }

    if (!reordered.empty() || found)
    {
        reordered.insert(
            reordered.begin(),
            "environment");
    }

    if (is_environment_first(plugins))
    {
        return false;
    }

    if (!rewrite_plugins(
            config_file,
            reordered,
            plugins_line))
    {
        return false;
    }

    return true;
}

void restart_wayfire()
{
    auto& core = wf::get_core();

    const pid_t parent_pid = getpid();

    const pid_t child = fork();

    if (child == 0)
    {
        /*
         * Wait until the current Wayfire process has
         * completely terminated.
         */
        while (kill(parent_pid, 0) == 0)
        {
            usleep(10000);
        }

        std::vector<char*> argv;

        for (int i = 0; i < core.argc; ++i)
        {
            argv.push_back(core.argv[i]);
        }

        argv.push_back(nullptr);

        execvp(argv[0], argv.data());

        _exit(127);
    }

    if (child < 0)
    {
        return;
    }

    /*
     * The child will restart Wayfire after this process
     * has completely terminated.
     */
    core.shutdown();
}

std::string expand_variables(std::string value)
{
    std::size_t pos = 0;
    while ((pos = value.find('$', pos)) != std::string::npos)
    {
        if (pos + 1 >= value.size())
        {
            break;
        }

        // Değişken adının sonunu bul (harf, rakam ve alt çizgi dahil)
        std::size_t start = pos + 1;
        std::size_t end = start;
        while (end < value.size() && 
               (std::isalnum(static_cast<unsigned char>(value[end])) || value[end] == '_'))
        {
            ++end;
        }

        if (end > start)
        {
            std::string var_name = value.substr(start, end - start);
            const char* env_val = std::getenv(var_name.c_str());
            std::string replacement = env_val ? env_val : "";

            // $DEĞİŞKEN kısmını gerçek değeriyle değiştir
            value.replace(pos, end - pos, replacement);
            pos += replacement.length();
        }
        else
        {
            // Sadece tek başına bir $ varsa atla
            pos++;
        }
    }

    return value;
}

void apply_environment()
{
    const std::string config_file =
        find_config_file();

    if (config_file.empty())
    {
        return;
    }

    std::ifstream file(config_file);

    if (!file.is_open())
    {
        return;
    }

    bool in_environment = false;

    std::vector<std::string> variables;

    std::string line;

    while (std::getline(file, line))
    {
        line = trim(line);

        if (line.empty())
        {
            continue;
        }

        if (line[0] == '#' || line[0] == ';')
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            const std::string section =
                trim(line.substr(1, line.size() - 2));

            in_environment =
                (section == "environment");

            continue;
        }

        if (!in_environment)
        {
            continue;
        }

        const auto equal = line.find('=');

        if (equal == std::string::npos)
        {
            continue;
        }

        const std::string key =
            trim(line.substr(0, equal));

        std::string value =
            trim(line.substr(equal + 1));

        if (!valid_name(key))
        {
            continue;
        }

        if (value.size() >= 2 &&
            (
                (value.front() == '"' &&
                 value.back() == '"') ||
                (value.front() == '\'' &&
                 value.back() == '\'')
            ))
        {
            value =
                value.substr(1, value.size() - 2);
        }

        value = expand_variables(value);

        if (setenv(
                key.c_str(),
                value.c_str(),
                1) == 0)
        {
            variables.push_back(key);
        }
    }

    update_activation_environment(variables);
}

}

class wayfire_environment :
    public wf::plugin_interface_t
{
public:

    void init() override
    {
        /*
         * Make sure that this plugin is the first plugin
         * loaded by Wayfire.
         */
        if (ensure_environment_first())
        {
            /*
             * The configuration has changed.
             *
             * Restart the compositor so that the new plugin
             * order is used from the very beginning.
             */
            restart_wayfire();
            return;
        }

        /*
         * The plugin is already first.
         *
         * Apply the environment normally.
         */
        apply_environment();
    }
};

DECLARE_WAYFIRE_PLUGIN(wayfire_environment);
