Import("env")

env.Append(CXXFLAGS=["-Wpedantic", "-Wno-register", "-Wno-write-strings"])
