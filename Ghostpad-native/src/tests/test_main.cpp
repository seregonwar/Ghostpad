// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

#include "protocol/gpad_packet.h"
#include "storage/console_store.h"
#include "storage/settings_store.h"
#include "storage/profile_store.h"
#include "storage/project_store.h"
#include "network/network_scanner.h"

#define ASSERT_TRUE(x) do { if (!(x)) { std::cerr << "Assertion failed: " << #x << " at " << __FILE__ << ":" << __LINE__ << std::endl; std::exit(1); } } while (0)
#define ASSERT_EQ(x, y) ASSERT_TRUE((x) == (y))
#define ASSERT_NE(x, y) ASSERT_TRUE((x) != (y))

void test_gpad_packets() {
    using namespace ghostpad;

    // Test buildGpadState
    PadStateInput input;
    input.button_states[0] = true; // Cross button
    input.stick_states[0] = 100;   // lx
    input.stick_states[1] = 200;   // ly
    input.trigger_l2 = 50;

    GpadNetworkState state = buildGpadState(input);
    ASSERT_EQ(state.lx, 100);
    ASSERT_EQ(state.ly, 200);
    ASSERT_EQ(state.l2, 50);
    ASSERT_TRUE((state.buttons & BTN_CROSS) != 0);
    ASSERT_TRUE((state.buttons & BTN_L2) != 0);

    // Test buildGpadPacket
    auto pkt = buildGpadPacket(state);
    ASSERT_EQ(pkt[0], 'G');
    ASSERT_EQ(pkt[1], 'P');
    ASSERT_EQ(pkt[2], 'A');
    ASSERT_EQ(pkt[3], 'D');
    ASSERT_EQ(pkt[8], 100);
    ASSERT_EQ(pkt[9], 200);
    ASSERT_EQ(pkt[12], 50);

    // Test buildTypePacket
    auto type_pkt = buildTypePacket(4);
    ASSERT_EQ(type_pkt[0], 'T');
    ASSERT_EQ(type_pkt[1], 'Y');
    ASSERT_EQ(type_pkt[2], 'P');
    ASSERT_EQ(type_pkt[3], 'E');
    ASSERT_EQ(type_pkt[4], 4);

    // Test buildGbndPacket
    auto gbnd_pkt = buildGbndPacket(0, 0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL);
    ASSERT_EQ(gbnd_pkt[0], 'G');
    ASSERT_EQ(gbnd_pkt[1], 'B');
    ASSERT_EQ(gbnd_pkt[2], 'N');
    ASSERT_EQ(gbnd_pkt[3], 'D');
    ASSERT_EQ(gbnd_pkt[4], 0);
    ASSERT_EQ(gbnd_pkt[8], 0x88);
    ASSERT_EQ(gbnd_pkt[15], 0x11);
    ASSERT_EQ(gbnd_pkt[16], 0x00);
    ASSERT_EQ(gbnd_pkt[23], 0x99);

    // Test buildDiscPacket & buildUnptPacket
    auto disc_pkt = buildDiscPacket();
    ASSERT_EQ(disc_pkt[0], 'D');
    ASSERT_EQ(disc_pkt[1], 'I');
    ASSERT_EQ(disc_pkt[2], 'S');
    ASSERT_EQ(disc_pkt[3], 'C');

    auto unpt_pkt = buildUnptPacket();
    ASSERT_EQ(unpt_pkt[0], 'U');
    ASSERT_EQ(unpt_pkt[1], 'N');
    ASSERT_EQ(unpt_pkt[2], 'P');
    ASSERT_EQ(unpt_pkt[3], 'T');

    std::cout << "test_gpad_packets passed!" << std::endl;
}

void test_network_scanner() {
    using namespace ghostpad;
    std::string subnet = NetworkScanner::getSubnetFromAddress("192.168.1.15");
    ASSERT_EQ(subnet, "192.168.1");

    std::string bad_subnet = NetworkScanner::getSubnetFromAddress("invalid_address");
    ASSERT_EQ(bad_subnet, "192.168.1");

    std::cout << "test_network_scanner passed!" << std::endl;
}

void test_stores() {
    using namespace ghostpad;
    std::string temp_dir = "./temp_test_config";
    std::filesystem::remove_all(temp_dir);

    // ConsoleStore
    {
        ConsoleStore store(temp_dir);
        auto list = store.list();
        ASSERT_EQ(list.size(), 0U);

        auto added = store.add("Test Console", "192.168.1.50", 6967, 9021);
        ASSERT_EQ(added.name, "Test Console");
        ASSERT_EQ(added.ip, "192.168.1.50");
        ASSERT_NE(added.id, "");

        list = store.list();
        ASSERT_EQ(list.size(), 1U);
        ASSERT_EQ(list[0].id, added.id);

        ConsoleEntry patch;
        patch.name = "Updated Console";
        auto updated = store.update(added.id, patch);
        ASSERT_EQ(updated.name, "Updated Console");
        ASSERT_EQ(updated.ip, "192.168.1.50");

        list = store.list();
        ASSERT_EQ(list[0].name, "Updated Console");

        bool deleted = store.remove(added.id);
        ASSERT_TRUE(deleted);
        list = store.list();
        ASSERT_EQ(list.size(), 0U);
    }

    // SettingsStore
    {
        SettingsStore store(temp_dir);
        auto current = store.read();
        ASSERT_TRUE(current.payload_elf_path.empty());

        AppSettings patch;
        patch.payload_elf_path = "some_elf.elf";
        patch.auto_deploy_on_connect = true;
        auto updated = store.write(patch);
        ASSERT_EQ(updated.payload_elf_path, "some_elf.elf");
        ASSERT_TRUE(updated.auto_deploy_on_connect);

        current = store.read();
        ASSERT_EQ(current.payload_elf_path, "some_elf.elf");
        ASSERT_TRUE(current.auto_deploy_on_connect);
    }

    // ProfileStore
    {
        ProfileStore store(temp_dir);
        auto list = store.list();
        ASSERT_EQ(list.size(), 0U);

        ProfileBindingEntry entry;
        entry.name = "Keyboard Profile";
        entry.mouse_sensitivity = 4.5f;
        auto added = store.add(entry);
        ASSERT_EQ(added.name, "Keyboard Profile");
        ASSERT_EQ(added.mouse_sensitivity, 4.5f);

        list = store.list();
        ASSERT_EQ(list.size(), 1U);

        ProfileBindingEntry retrieved = store.get(added.id);
        ASSERT_EQ(retrieved.name, "Keyboard Profile");

        ProfileBindingEntry patch;
        patch.name = "Pro Profile";
        auto updated = store.update(added.id, patch);
        ASSERT_EQ(updated.name, "Pro Profile");

        bool deleted = store.remove(added.id);
        ASSERT_TRUE(deleted);
        list = store.list();
        ASSERT_EQ(list.size(), 0U);
    }

    // ProjectStore
    {
        ProjectStore store(temp_dir);
        auto list = store.list();
        ASSERT_EQ(list.size(), 0U);

        auto added = store.add("Test Project", "Test Description", "Test Game");
        ASSERT_EQ(added.name, "Test Project");
        ASSERT_EQ(added.description, "Test Description");

        list = store.list();
        ASSERT_EQ(list.size(), 1U);

        // Test Macro Command inside Project
        MacroCommand cmd;
        cmd.name = "Combo 1";
        cmd.signals = { {0, 255, 10}, {0, 0, 50} };
        bool cmd_added = store.addCommand(added.id, cmd);
        ASSERT_TRUE(cmd_added);

        auto retrieved = store.get(added.id);
        ASSERT_EQ(retrieved.commands.size(), 1U);
        ASSERT_EQ(retrieved.commands[0].name, "Combo 1");
        ASSERT_EQ(retrieved.commands[0].signals.size(), 2U);

        std::string cmd_id = retrieved.commands[0].id;
        MacroCommand cmd_patch;
        cmd_patch.id = cmd_id;
        cmd_patch.name = "Super Combo 1";
        cmd_patch.signals = { {0, 255, 10} };
        bool cmd_updated = store.updateCommand(added.id, cmd_patch);
        ASSERT_TRUE(cmd_updated);

        retrieved = store.get(added.id);
        ASSERT_EQ(retrieved.commands[0].name, "Super Combo 1");
        ASSERT_EQ(retrieved.commands[0].signals.size(), 1U);

        bool cmd_removed = store.removeCommand(added.id, cmd_id);
        ASSERT_TRUE(cmd_removed);

        retrieved = store.get(added.id);
        ASSERT_EQ(retrieved.commands.size(), 0U);

        // Cleanup
        bool deleted = store.remove(added.id);
        ASSERT_TRUE(deleted);
    }

    std::filesystem::remove_all(temp_dir);
    std::cout << "test_stores passed!" << std::endl;
}

int main() {
    std::cout << "====================================================" << std::endl;
    std::cout << "          STARTING GHOSTPAD NATIVE UNIT TESTS       " << std::endl;
    std::cout << "====================================================" << std::endl;

    test_gpad_packets();
    test_network_scanner();
    test_stores();

    std::cout << "====================================================" << std::endl;
    std::cout << "             ALL TESTS COMPLETED SUCCESSFULLY!      " << std::endl;
    std::cout << "====================================================" << std::endl;

    return 0;
}
