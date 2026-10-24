from mininet.net import Mininet
from mininet.node import RemoteController, OVSSwitch
from mininet.link import TCLink
from mininet.cli import CLI
from mininet.log import setLogLevel
import subprocess
import time

CONTROLLER_IP = "100.89.53.8"

LOCAL_VM_IP = "100.67.173.41"
VM3_IP = "100.76.78.70"


def add_gre_tunnel():

    subprocess.run(
        "ovs-vsctl --if-exists del-port s2 gre0",
        shell=True,
        check=True
    )

    command = (
        f"ovs-vsctl add-port s2 gre0 "
        f"-- set Interface gre0 "
        f"type=gre "
        f"options:local_ip={LOCAL_VM_IP} "
        f"options:remote_ip={VM3_IP} "
        f"ofport_request=4"
    )

    subprocess.run(
        command,
        shell=True,
        check=True
    )

    print("GRE tunnel created on s2")

    result = subprocess.run(
        "ovs-vsctl get Interface gre0 ofport",
        shell=True,
        capture_output=True,
        text=True
    )

    print("GRE port:", result.stdout.strip())


def create_network():

    net = Mininet(
        controller=None,
        switch=OVSSwitch,
        link=TCLink,
        autoSetMacs=False
    )

    c0 = net.addController(
        "c0",
        controller=RemoteController,
        ip=CONTROLLER_IP,
        port=6633
    )

    s1 = net.addSwitch(
        "s1",
        dpid="0000000000000001",
        protocols="OpenFlow13"
    )

    s2 = net.addSwitch(
        "s2",
        dpid="0000000000000002",
        protocols="OpenFlow13"
    )

    h1 = net.addHost(
        "h1",
        ip="10.10.0.1/24",
        mac="00:00:00:00:00:01"
    )

    h2 = net.addHost(
        "h2",
        ip="10.10.0.2/24",
        mac="00:00:00:00:00:02"
    )

    h3 = net.addHost(
        "h3",
        ip="10.10.0.3/24",
        mac="00:00:00:00:00:03"
    )

    hbg = net.addHost(
        "hbg",
        ip="10.10.0.50/24",
        mac="00:00:00:00:00:50"
    )

    net.addLink(h1, s1, port1=1, port2=1)
    net.addLink(h2, s1, port1=1, port2=2)
    net.addLink(h3, s1, port1=1, port2=3)
    net.addLink(hbg, s1, port1=1, port2=4)

    # Slow path
    net.addLink(
        s1,
        s2,
        port1=5,
        port2=2,
        cls=TCLink,
        bw=5,
        delay="50ms",
        max_queue_size=1000
    )

    # Fast path
    net.addLink(
        s1,
        s2,
        port1=6,
        port2=3,
        cls=TCLink,
        bw=20,
        delay="5ms",
        max_queue_size=200
    )

    net.build()

    c0.start()

    s1.start([c0])
    s2.start([c0])

    time.sleep(2)

    add_gre_tunnel()

    print("\nClient-side network started.")
    print("h1 = 10.10.0.1")
    print("h2 = 10.10.0.2")
    print("h3 = 10.10.0.3")
    print("hbg = 10.10.0.50")
    print()
    print("Slow path: s1 port 5 -> s2 port 2")
    print("Fast path: s1 port 6 -> s2 port 3")
    print("GRE: s2 port 4")
    print()

    CLI(net)

    net.stop()


if __name__ == "__main__":

    setLogLevel("info")

    try:
        create_network()
    except KeyboardInterrupt:
        pass
    finally:
        subprocess.run(
            "mn -c",
            shell=True
        )
