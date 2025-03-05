# rmw_zenoh

[![build](https://github.com/ros2/rmw_zenoh/actions/workflows/build.yaml/badge.svg)](https://github.com/ros2/rmw_zenoh/actions/workflows/build.yaml)
[![style](https://github.com/ros2/rmw_zenoh/actions/workflows/style.yaml/badge.svg)](https://github.com/ros2/rmw_zenoh/actions/workflows/style.yaml)

A ROS 2 RMW implementation based on Zenoh that is written using the zenoh-cpp bindings.

## Design

For information about the Design please visit [design](docs/design.md) page.

## Requirements
- [ROS 2](https://docs.ros.org)

> Note: See available distro branches, eg. `jazzy`, for supported ROS 2 distributions.

## Installation
`rmw_zenoh` can either be installed via binaries (recommended for stable development) or built from source (recommended if latest features are needed). See instructions below.

### Binary Installation
Binary packages for supported ROS 2 distributions (see distro branches) are available on respective [Tier-1](https://www.ros.org/reps/rep-2000.html#support-tiers) platforms for the distributions.
First ensure that your system is set up to install ROS 2 binaries by following the instructions [here](https://docs.ros.org/en/rolling/Installation/Ubuntu-Install-Debs.html).

Then install `rmw_zenoh` binaries using the command

```bash
sudo apt update && sudo apt install ros-<DISTRO>-rmw-zenoh-cpp # replace <DISTRO> with the codename for the distribution, eg., rolling
```

### Source Installation

>Note: By default, we vendor and compile `zenoh-cpp` with a subset of `zenoh` features.
The `ZENOHC_CARGO_FLAGS` CMake argument may be overwritten with other features included if required.
See [zenoh_cpp_vendor/CMakeLists.txt](./zenoh_cpp_vendor/CMakeLists.txt) for more details.

```bash
# replace <DISTRO> with ROS 2 distro of choice
mkdir ~/ws_rmw_zenoh/src -p && cd ~/ws_rmw_zenoh/src
git clone https://github.com/ros2/rmw_zenoh.git -b <DISTRO>
cd ~/ws_rmw_zenoh
rosdep install --from-paths src --ignore-src --rosdistro <DISTRO> -y
source /opt/ros/<DISTRO>/setup.bash
colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release
```

Make sure to source the built workspace using the commands below prior to running any other commands.
```bash
cd ~/ws_rmw_zenoh
source install/setup.bash
```

## Test

### Terminate ROS 2 daemon started with another RMW
```bash
pkill -9 -f ros && ros2 daemon stop
```
Without this step, ROS 2 CLI commands (e.g. `ros2 node list`) may
not work properly since they would query ROS graph information from the ROS 2 daemon that
may have been started with different a RMW.

### Start the Zenoh router
> Note: Manually launching Zenoh router won't be necessary in the future.
```bash
# terminal 1
ros2 run rmw_zenoh_cpp rmw_zenohd
```

> Note: Without the Zenoh router, nodes will not be able to discover each other since multicast discovery is disabled by default in the node's session config. Instead, nodes will receive discovery information about other peers via the Zenoh router's gossip functionality. See more information on the session configs [below](#configuration).

### Run the `talker`
```bash
# terminal 2
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
ros2 run demo_nodes_cpp talker
```

### Run the `listener`
```bash
# terminal 2
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
ros2 run demo_nodes_cpp listener
```

The listener node should start receiving messages over the `/chatter` topic.

## Configuration

By default, `Zenoh sessions` created by `rmw_zenoh` will attempt to connect to a Zenoh router to receive discovery information.
To understand more about `Zenoh routers` and `Zenoh sessions`, see [Zenoh documentation](https://zenoh.io/docs/getting-started/deployment/).

### Checking for a Zenoh router.
The `ZENOH_ROUTER_CHECK_ATTEMPTS` environment variable can be used to configure if and how a `Zenoh session` checks for the presence of a `Zenoh router`.
The behavior is explained in the table below.


| ZENOH_ROUTER_CHECK_ATTEMPTS |                                                 Session behavior                                                   |
|:---------------------------:|:------------------------------------------------------------------------------------------------------------------:|
|               0             |                                                             Indefinitely waits for connection to a Zenoh router.   |
|             < 0             |                                                                                        Skips Zenoh router check.   |
|             > 0             | Attempts to connect to a Zenoh router in `ZENOH_ROUTER_CHECK_ATTEMPTS` attempts with 1 second wait between checks. |
|            unset            |                                                                    Equivalent to `1`: the check is made only once. |

If after the configured number of attempts the Node is still not connected to a `Zenoh router`, the initialisation goes on anyway.
If a `Zenoh router` is started after initialization phase, the Node will automatically connect to it, and autoconnect to other Nodes if gossip scouting is enabled (true with default configuratiuon).

### Session and Router configs
`rmw_zenoh` relies on separate configurations files to configure the `Zenoh router` and `Zenoh session` respectively.
For more information on the topology of Zenoh adopted in `rmw_zenoh`, please see [Design](#design).
Default configuration files are used by `rmw_zenoh` however certain environment variables may be set to provide absolute paths to custom configuration files.
The table below summarizes the default files and the environment variables for the `Zenoh router` and `Zenoh session`.
For a complete list of configurable parameters, see [zenoh/DEFAULT_CONFIG.json5](https://github.com/eclipse-zenoh/zenoh/blob/main/DEFAULT_CONFIG.json5).

|         |                                            Default config                                            |   Envar for custom config  |
|---------|:----------------------------------------------------------------------------------------------------:|:--------------------------:|
| Router  |  [DEFAULT_RMW_ZENOH_ROUTER_CONFIG.json5](rmw_zenoh_cpp/config/DEFAULT_RMW_ZENOH_ROUTER_CONFIG.json5)  |  `ZENOH_ROUTER_CONFIG_URI` |
| Session | [DEFAULT_RMW_ZENOH_SESSION_CONFIG.json5](rmw_zenoh_cpp/config/DEFAULT_RMW_ZENOH_SESSION_CONFIG.json5) | `ZENOH_SESSION_CONFIG_URI` |

For example, to set the path to a custom `Zenoh router` configuration file,
```bash
export ZENOH_ROUTER_CONFIG_URI=$HOME/MY_ZENOH_ROUTER_CONFIG.json5
```

### Connecting multiple hosts
By default, all discovery & communication is restricted within a host, where a host is a machine running a `Zenoh router` along with various ROS 2 nodes with their default [configurations](rmw_zenoh_cpp/config/).
To bridge communications across two or more hosts, the `Zenoh router` configuration for one of the hosts must be updated to connect to the other host's `Zenoh router` at startup.

First, make a copy of the [DEFAULT_RMW_ZENOH_ROUTER_CONFIG.json5](rmw_zenoh_cpp/config/DEFAULT_RMW_ZENOH_ROUTER_CONFIG.json5) and modify the `connect` block to include the endpoint(s) that the other host's `Zenoh router(s)` is listening on.
For example, if another `Zenoh router` is listening on IP address `192.168.1.1` and port `7447` on its host, modify the config file to connect to this router as shown below:

```json5
/// ... preceding parts of the config file.
{
  connect: {
    endpoints: ["tcp/192.168.1.1:7447"],
  },
}
/// ... following parts of the config file.
```

Then, start the `Zenoh router` after setting the `ZENOH_ROUTER_CONFIG_URI` environment variable to the absolute path of the modified config file.

### Logging

The core of Zenoh is implemented in Rust and uses a logging library that can be configured via a `RUST_LOG` environment variable.
This variable can be configured independently for each Node and the Zenoh router.
For instance:
- `RUST_LOG=zenoh=info` activates information logs about Zenoh initialization and the endpoints it's listening on.
- `RUST_LOG=zenoh=info,zenoh_transport=debug` adds some debug logs about the connectivity events in Zenoh.
- `RUST_LOG=zenoh=info,zenoh::net::routing::queries=trace` adds some trace logs for each query (i.e. calls to services and actions).
- `RUST_LOG=zenoh=debug` activates all the debug logs.

For more information on the `RUST_LOG` syntax, see https://docs.rs/env_logger/latest/env_logger/#enabling-logging.

### Known Issues

### Router crashes on IPv4-only systems

The default configuration shipped with `rmw_zenoh` makes the Zenoh router attempt to listen on IPv6 `ANY` only ([here](https://github.com/ros2/rmw_zenoh/blob/12f83445e00a7c25805b27f391e92a455f2d0774/rmw_zenoh_cpp/config/DEFAULT_RMW_ZENOH_ROUTER_CONFIG.json5#L82-L84)).
On any system without IPv6 support (either because it has been disabled or because it is non-functioning) this will cause the router to crash with an error similar to the following:

<details><summary>Click to expand</summary>

```
WARN ThreadId(03) zenoh::net::runtime::orchestrator: Unable to open listener tcp/[::]:7447: Can not create a new TCP listener bound to tcp/[::]:7447: [Os { code: 97, kind: Uncategorized, message: "Address family not supported by protocol" }] at /home/buildfarm/.cargo/git/checkouts/zenoh-cc237f2570fab813/9640d22/io/zenoh-links/zenoh-link-tcp/src/unicast.rs:326.
ERROR ThreadId(03) zenohc::session: Error opening session: Can not create a new TCP listener bound to tcp/[::]:7447: [Os { code: 97, kind: Uncategorized, message: "Address family not supported by protocol" }] at /home/buildfarm/.cargo/git/checkouts/zenoh-cc237f2570fab813/9640d22/io/zenoh-links/zenoh-link-tcp/src/unicast.rs:326.
Error opening Session!\n[ros2run]: Process exited with failure 1
```

</details>

To resolve this, either run the router on a system with IPv6 support, or update the `listen.endpoints` list in the Zenoh configuration and replace `"tcp/[::]:7447"` with `"tcp/0.0.0.0:7447"` (ie: make the router listen on IPv4 `ANY`).
Note: the existing entry must be *replaced*, it's not sufficient to just *add* the IPv4 entry (as that would make the router attempt to listen on both IPv4 and IPv6 `ANY` and still not work).

### Crash when program terminates

When a program terminates, global and static objects are destructed in the reverse order of their
construction.
The `Thread Local Storage` is one such entity which the `tokio` runtime in Zenoh uses.
If the Zenoh session is closed after this entity is cleared, it causes a panic like seen below.

```
thread '<unnamed>' panicked at /rustc/aedd173a2c086e558c2b66d3743b344f977621a7/library/std/src/thread/local.rs:262:26:
cannot access a Thread Local Storage value during or after destruction: AccessError
```

This can happen with `rmw_zenoh` if the ROS 2 `Context` is not shutdown explicitly before the
program terminates.
In this scenario, the `Context` will be shutdown inside the `Context`'s destructor which then closes the Zenoh session.
Since the ordering of global/static objects is not defined, this often leads to the above panic.

The recommendation is to ensure the `Context` is shutdown before a program terminates.
One way to ensure this is to call `rclcpp::shutdown()` when the program exits.
Note that composable nodes should *never* call `rclcpp::shutdown()`, as the composable node container will automatically do this.

For more details, see https://github.com/ros2/rmw_zenoh/issues/170.
