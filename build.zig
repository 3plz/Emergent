const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    mod.addCSourceFiles(.{
        .files = &.{
            "src/main.c",
            "src/utils/file.c",
            "src/utils/strbuf.c",
            "src/lexer/lexer.c",
            "src/parser/ast.c",
            "src/parser/parser.c",
            "src/parser/module.c",
            "src/codegen/types.c",
            "src/codegen/pool.c",
            "src/codegen/scope.c",
            "src/codegen/func_table.c",
            "src/codegen/codegen.c",
            "src/driver/driver.c",
            "src/doc/doc_man.c",
        },
        .flags = &.{
            "-Wall",
            "-Wextra",
            "-std=c11",
        },
    });

    mod.addIncludePath(b.path("src"));

    const exe = b.addExecutable(.{
        .name = "emc",
        .root_module = mod,
    });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the compiler");
    run_step.dependOn(&run_cmd.step);

    const tests = b.addSystemCommand(&.{"python3"});
    tests.addFileArg(b.path("tests/test_compiler.py"));
    tests.addArtifactArg(exe);
    const test_step = b.step("test", "Run compiler integration tests");
    test_step.dependOn(&tests.step);
}
