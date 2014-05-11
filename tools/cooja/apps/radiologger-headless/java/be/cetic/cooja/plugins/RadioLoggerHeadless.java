/*
 * Copyright (c) 2013, CETIC.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

package be.cetic.cooja.plugins;

import java.io.File;
import java.io.IOException;
import java.util.Observable;
import java.util.Observer;
import java.util.Properties;
import java.util.ArrayList;

import org.apache.log4j.Logger;

import org.contikios.cooja.ClassDescription;
import org.contikios.cooja.ConvertedRadioPacket;
import org.contikios.cooja.Cooja;
import org.contikios.cooja.Plugin;
import org.contikios.cooja.PluginType;
import org.contikios.cooja.RadioConnection;
import org.contikios.cooja.RadioMedium;
import org.contikios.cooja.RadioPacket;
import org.contikios.cooja.Simulation;
import org.contikios.cooja.VisPlugin;
import org.contikios.cooja.interfaces.Radio;
import org.contikios.cooja.plugins.analyzers.PacketAnalyzer;
import org.contikios.cooja.plugins.analyzers.ICMPv6Analyzer;
import org.contikios.cooja.plugins.analyzers.IEEE802154Analyzer;
import org.contikios.cooja.plugins.analyzers.IPHCPacketAnalyzer;
import org.contikios.cooja.plugins.analyzers.IPv6PacketAnalyzer;

import org.contikios.cooja.util.StringUtils;

/**
 * Radio Logger which writes log to file when closed.
 * It was designed to support radio logging in COOJA's headless mode.
 * Based on RadioLoggerHeadless.java by Laurent Deru and RadioLogger.java by Fredrik Osterlind
 *
 * @author Philipp Raich
 */
@ClassDescription("Headless radio logger")
@PluginType(PluginType.SIM_PLUGIN)
public class RadioLoggerHeadless extends VisPlugin {
  private static Logger logger = Logger.getLogger(RadioLoggerHeadless.class);
  private static final long serialVersionUID = -6927099123477081353L;

  private final Simulation simulation;
  private RadioMedium radioMedium;
  private Observer radioMediumObserver;

  ArrayList<PacketAnalyzer> analyzers = new ArrayList<PacketAnalyzer>();
  ArrayList<RadioConnectionLog> connections = new ArrayList<RadioConnectionLog>();

  public RadioLoggerHeadless(final Simulation simulationToControl, final Cooja gui) {
    super("Radio messages", gui, false);
    logger.info("Starting headless radio logger");

    simulation = simulationToControl;
    radioMedium = simulation.getRadioMedium();

    analyzers.add(new IEEE802154Analyzer(false));
    analyzers.add(new IPHCPacketAnalyzer());
    analyzers.add(new IPv6PacketAnalyzer());
    analyzers.add(new ICMPv6Analyzer());

    radioMedium.addRadioMediumObserver(radioMediumObserver = new Observer() {
      public void update(Observable obs, Object obj) {
        RadioConnection conn = radioMedium.getLastConnection();
        if (conn == null) {
          return;
        }
        final RadioConnectionLog loggedConn = new RadioConnectionLog();
        loggedConn.startTime = conn.getStartTime();
        loggedConn.endTime = simulation.getSimulationTime();
        loggedConn.connection = conn;
        loggedConn.packet = conn.getSource().getLastPacketTransmitted();

        connections.add(loggedConn);
      }
    });
  }

  public void closePlugin() {
    if (radioMediumObserver != null) {
      radioMedium.deleteRadioMediumObserver(radioMediumObserver);
    }
    if (!connections.isEmpty()) {
      File file = new File("COOJA.radiolog");
      if (file.exists()) {
        logger.warn("overwriting old output file!");
      }
      StringUtils.saveToFile(file, getConnectionsString());
    }
  }

  public String getConnectionsString() {
    StringBuilder sb = new StringBuilder();
    RadioConnectionLog[] cs = connections.toArray(new RadioConnectionLog[0]);
    for(RadioConnectionLog c: cs) {
      sb.append(c.toString() + "\n");
    }
    return sb.toString();
  };

  private class RadioConnectionLog {
    long startTime;
    long endTime;
    RadioConnection connection;
    RadioPacket packet;

    RadioConnectionLog hiddenBy = null;
    int hides = 0;

    String data = null;
    String tooltip = null;

    public String toString() {
      if (data == null) {
        RadioLoggerHeadless.this.prepareDataString(this);
      }
      return
        Long.toString(startTime / Simulation.MILLISECOND) + "\t" +
        connection.getSource().getMote().getID() + "\t" +
        getDestString(this) + "\t" +
        data;
    }
  }

  private static String getDestString(RadioConnectionLog c) {
    Radio[] dests = c.connection.getDestinations();
    if (dests.length == 0) {
      return "-";
    }
    if (dests.length == 1) {
      return "" + dests[0].getMote().getID();
    }
    StringBuilder sb = new StringBuilder();
    for (Radio dest: dests) {
      sb.append(dest.getMote().getID()).append(',');
    }
    sb.setLength(sb.length()-1);
    return sb.toString();
  }

  private void prepareDataString(RadioConnectionLog conn) {
    byte[] data;
    if (conn.packet == null) {
      data = null;
    } else if (conn.packet instanceof ConvertedRadioPacket) {
      data = ((ConvertedRadioPacket)conn.packet).getOriginalPacketData();
    } else {
      data = conn.packet.getPacketData();
    }
    if (data == null) {
      conn.data = "[unknown data]";
      return;
    }

    StringBuffer brief = new StringBuffer();
    StringBuffer verbose = new StringBuffer();

    /* default analyzer */
    PacketAnalyzer.Packet packet = new PacketAnalyzer.Packet(data, PacketAnalyzer.MAC_LEVEL);

    if (analyzePacket(packet, brief, verbose)) {
      if (packet.hasMoreData()) {
        byte[] payload = packet.getPayload();
        brief.append(StringUtils.toHex(payload, 4));
        if (verbose.length() > 0) {
          verbose.append("<p>");
        }
        verbose.append("<b>Payload (")
          .append(payload.length).append(" bytes)</b><br><pre>")
          .append(StringUtils.hexDump(payload))
          .append("</pre>");
      }
      conn.data = (data.length < 100 ? (data.length < 10 ? "  " : " ") : "")
        + data.length + ": " + brief;
      if (verbose.length() > 0) {
        conn.tooltip = verbose.toString();
      }
    } else {
      conn.data = data.length + ": 0x" + StringUtils.toHex(data, 4);
    }
  }

  private boolean analyzePacket(PacketAnalyzer.Packet packet, StringBuffer brief, StringBuffer verbose) {
    if (analyzers == null) return false;
    try {
      boolean analyze = true;
      while (analyze) {
        analyze = false;
        for (int i = 0; i < analyzers.size(); i++) {
          PacketAnalyzer analyzer = analyzers.get(i);
          if (analyzer.matchPacket(packet)) {
            int res = analyzer.analyzePacket(packet, brief, verbose);
            if (packet.hasMoreData() && brief.length() > 0) {
              brief.append('|');
              verbose.append("<br>");
            }
            if (res != PacketAnalyzer.ANALYSIS_OK_CONTINUE) {
              /* this was the final or the analysis failed - no analyzable payload possible here... */
              return brief.length() > 0;
            }
            /* continue another round if more bytes left */
            analyze = packet.hasMoreData();
            break;
          }
        }
      }
    } catch (Exception e) {
      logger.debug("Error when analyzing packet: " + e.getMessage(), e);
      return false;
    }
    return brief.length() > 0;
  }
}
