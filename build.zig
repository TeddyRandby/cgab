const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const libcgab = b.addStaticLibrary(.{
        .name = "cgab",
        .target = target,
        .optimize = optimize,
        .pic = true,
    });

    libcgab.addIncludePath(b.path("include"));

    libcgab.addCSourceFiles(.{
        .files = &.{
            "src/cgab/gc.c",
            "src/cgab/vm.c",
            "src/cgab/lexer.c",
            "src/cgab/engine.c",
            "src/cgab/object.c",
            "src/cgab/parser.c",
        },
        .flags = &.{
            "-std=c2x",
            "-march=native",
            "-MMD",
            "-Wall",
            "-fno-sanitize=all",
        },
    });

    libcgab.linkLibC();

    const gab = b.addExecutable(.{
        .name = "gab",
        .target = target,
        .optimize = optimize,
        .pic = true,
    });

    gab.addIncludePath(b.path("include"));
    gab.addCSourceFiles(.{
        .files = &.{ "src/gab/main.c", "src/os/os.c" },
        .flags = &.{
            "-std=c2x",
            "-march=native",
            "-MMD",
            "-Wall",
            "-fno-sanitize=all",
        },
    });

    gab.linkLibC();
    gab.linkLibrary(libcgab);
    gab.linkSystemLibrary("ncurses");
    gab.linkSystemLibrary("readline");

    b.installArtifact(libcgab);
    b.installArtifact(gab);
}
