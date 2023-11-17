const std = @import("std");

const c = @cImport({
    @cInclude("xdp_loader.h");
    @cInclude("defs.h");
    @cInclude("stdlib.h");
});

pub fn main() !void {
    const veth_name = "test";
    if (!create_veth(veth_name)) {
        std.debug.print("Failed to create veth pair\n", .{});
        return;
    }
}

fn create_veth(veth_name: []const u8) bool {
    var cmd: [1024]u8 = undefined;
    const cmd_len = std.fmt.bufPrint(&cmd, "./testenv/testenv.sh setup --name={s}", .{veth_name}) catch return false;

    const cmd_ptr: [*c]const u8 = &cmd[0];

    const err = c.system(cmd_ptr);
    if (err != 0) {
        std.debug.print("Failed to create veth pair: {d}\n", .{err});
        return false;
    }

    return true;
}
