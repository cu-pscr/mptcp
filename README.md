# mptcp

The Client based MPTCP solution implementation in NS3.

To run, you have to install the DCE-NS3 
https://www.nsnam.org/docs/dce/manual/html/getting-started.html

DCE-NS3 uses the old version of iperf, replace the same with iperf3 by modyfuing the bakeconf.xml as below:

<module name="iperf">
      <source type="archive">
        <attribute name="url" value="https://downloads.es.net/pub/iperf/iperf-3.7.tar.gz"/>
        <attribute name="extract_directory" value="iperf-3.7"/>
      </source>
      <build type="make" objdir="yes">
        <attribute name="pre_installation" value="cd $SRCDIR;./configure --prefix=$INSTALLDIR"/>
        <attribute name="build_arguments" value="CFLAGS=-fPIC CFLAGS+=-U_FORTIFY_SOURCE CXXFLAGS=-fPIC CXXFLAGS+=-U_FORTIFY_SOURCE LDFLAGS=-pie LDFLAGS+=-rdynamic"/>
      </build>
     </module>
     
For using our client based MPTCP, you have to replace the net-next-nuse-4.4.0 with our given net-next-nuse that gives the MPTCP protocl stack files.

Once DCE-NS3 is running, use the attached dce-iperf-mptcp.cc file in <>/ns3/bake/source/ns-3-dce/example/ which captures multiple dynamic path situation.
