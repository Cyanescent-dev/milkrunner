#pragma once
#include <QFile>
#include <QJsonObject>
#if defined(Q_OS_UNIX)
#include <sys/resource.h>
#include <unistd.h>
#endif
inline QJsonObject processMetrics()
{
    QJsonObject result;
#if defined(Q_OS_UNIX)
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0)
    {
        result["cpu_seconds"] = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6 +
                                usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
#ifdef Q_OS_MACOS
        result["peak_rss_bytes"] = double(usage.ru_maxrss);
#else
        result["peak_rss_bytes"] = double(usage.ru_maxrss) * 1024;
#endif
    }
#ifdef Q_OS_LINUX
    QFile stat("/proc/self/statm");
    if (stat.open(QIODevice::ReadOnly))
    {
        auto fields = stat.readAll().simplified().split(' ');
        if (fields.size() > 1)
            result["rss_bytes"] = double(fields[1].toLongLong()) * sysconf(_SC_PAGESIZE);
    }
#endif
#endif
    return result;
}
