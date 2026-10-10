#pragma once
#include <string>
#include "../../XorStr.h"

namespace g_CheatData {
	namespace VTable {
		namespace D3D12 {
			inline int Present = 8;
			inline int ExecuteCommandLists = 10;
			inline int ResizeBuffers = 13;
		}

		namespace AActor {
			inline int Tick = 470;
		}

		namespace UMovementComponent {
			inline int PhysicsRotation = 318;
		}

		namespace UGameViewportClient {
			inline int PostRender = 122;
		}

		namespace UConsole {
			inline int OutputTextLine = 99;
		}
	}

	namespace Signature {
		namespace AShooterPlayerController {
			// 48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 55 48 8D 6C 24 ? 48 81 EC ? ? ? ? 80 65 ? ? 33 DB 0F

			// 应该不再需要这个东西了，没有用处，只是视觉效果
			inline std::string ClientChatMessage = _XOR_("OUTDATED").str();
     	}

		namespace APrimalPlayerController {
			
			// 他妈的特别特别特别长的特征码
			// 应该不再需要这个东西了，它似乎什么都没做，idk，我没有去检查它的call状态
			inline std::string ClientNotifyReconnected = _XOR_("OUTDATED").str();


			// 跳过重生动画
			inline std::string ClientNotifyRespawned = _XOR_("48 89 5C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC ? 33 FF 48 8B DA 48 89 7C 24 ? 4C 8B F1 E8 ? ? ? ? 48 8B 43 ? 4C 8D 3D ? ? ? ? 48 85 C0 74 ? 0F B6 08 4C 8D 44 24 ? 48 FF C0 48 8B D3 48 89 43 ? 4D 8B 0C CF 48 8B 4B ? 41 FF D1 EB ? 48 8B 8B ? ? ? ? 48 8B 41 ? 48 89 83 ? ? ? ? 8B 41 ? 48 0F BA E0 ? 73 ? 48 8B 83 ? ? ? ? 48 39 08 74 ? 0F 1F 40 ? 48 8B 40 ? 48 39 08 75 ? 48 8B 40 ? 48 89 43 ? 48 89 7B ? EB ? 48 63 41 ? 48 8D 54 24 ? 4C 8B 43 ? 49 03 C0 4C 89 43 ? 48 89 43 ? 48 8B 01 FF 90 ? ? ? ? B8 ? ? ? ? 89 7C 24 ? 48 8B CB 48 8D 34 18 E8 ? ? ? ? 48 8B 43 ? 48 85 C0 74 ? 0F B6 08 4C 8D 44 24 ? 48 FF C0 48 8B D3 48 89 43 ? 4D 8B 0C CF 48 8B 4B ? 41 FF D1 EB ? 48 8B 0E 48 8B 41 ? 48 89 06 8B 41 ? 48 0F BA E0 ? 73 ? 48 8B 83 ? ? ? ? 48 39 08 74 ? 48 8B 40 ? 48 39 08 75 ? 48 8B 40 ? 48 89 43 ? 48 89 7B ? EB ? 48 63 41 ? 48 8D 54 24 ? 4C 8B 43 ? 49 03 C0 4C 89 43 ? 48 89 43 ? 48 8B 01 FF 90 ? ? ? ? 48 8B 43 ? 49 8B CE 48 8B 54 24 ? 48 85 C0 40 0F 95 C7 48 03 F8 83 7C 24 ? ? 48 89 7B ? 49 8B 06 41 0F 95 C0 FF 90 ? ? ? ? 48 8B 5C 24 ? 48 8B 74 24 ? 48 83 C4 ? 41 5F 41 5E 5F C3 CC CC CC 48 83 EC ? 48 8B 05 ? ? ? ? 48 85 C0 75 ? 48 8D 15 ? ? ? ? 48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8B 05 ? ? ? ? 48 83 C4 ? C3 CC 48 8B C4 53 41").str();
		}

		namespace AActor {
			// push rbp | push rbx | push rsi | push rdi | push r12 | push r13 | push r14 | push r15 | lea rbp, [rsp+offset] | sub rsp, alloc | movaps [rsp+offset], xmm11
			// 无法在服务器工作，需要找到ASA重写的TakeDamage，或者UI Hit Damage，idk
			inline std::string TakeDamage = _XOR_("OUTDATED").str();

			// 470
			inline std::string Tick = _XOR_("40 53 48 83 EC ? 0F B6 81 ? ? ? ? 48 8B D9 A8 ? 75 ? A8 ? 74").str();
		}

		namespace UNetConnection {
			// 7FF7A8D25520
			// 也没啥用，退出服务器的时候会触发一次。
			// 不敢相信这个特征码能存活这么长时间
			inline std::string HandleDisconnect = _XOR_("? ? ? ? ? ? ? 55 57 48 8D 68 A1 48 81 EC D8 00 00 00 48 89 58 E8 48 8D 55 E7 48 89 70 E0").str();
		}

		namespace UMovementComponent {
			// 48 89 5C 24 ? 48 89 7C 24 ? 55 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 44 0F 29 BC 24 过期了
			// 2026/5/20 @zetsr
			// mov rax, rsp | push rbx | sub rsp, alloc | movaps [rax+offset], xmm6 | mov rbx, rcx | movaps [rax+offset], xmm7 | movaps
			// 48 8B C4 53 48 81 EC 00 01 00 00 0F 29 70 E8 48 8B D9 0F 29 78 D8 0F 28

			// 318
			inline std::string PhysicsRotation = _XOR_("48 8B C4 53 48 81 EC ? ? ? ? 0F 29 70 ? 48 8B D9 0F 29 78 ? 0F").str();
		}

		/*
		namespace UWorld {
			// 7FF7A935DA20 World Tick?
			// 7FF7A935DC40 World Post Tick?
			// 2026/4/3 @zetsr
			// ? ? ? ? ? 57 48 83 EC ? 48 8B 42 ? 49 8B E8 48 89 5C 24 ? 4C 8B E9 48 89 74 24 ? 48 8B 2026/4/3 过期了

			// 2026/4/3 @zetsr
			// push rbp push r14 push r15 sub rsp, offset mov rax, [rdx + offset] mov eax, [rdx + offset] mov [rsp + offset], rbx mov ebp, ecx mov [rsp + offset], rsi mov rax, [rdx]

			// 2026/4/26 @zetsr & @Lynneren
			// push rbx; push rbp; push rsi; push rdi; push r12; push r14; push r15; sub rsp, sz; mov rax, [cookie]; xor rax, rsp; mov [rsp+sz], rax; mov r14, rcx; mov r15, r8; lea rcx, [rsp+30h]
			// 40 53 55 56 57 41 54 41 56 41 57 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 ?? ?? ?? ?? 4C 8B F1 4D 8B F8 48 8D 4C 24 30 48

			// 2026/5/2 @zetsr
			// mov r1x, [mem] | push rbp | push r14 | push r15 | sub rsp, alloc | mov rax, [rip+offset]
			// 4C 8B ? 55 41 56 41 57 48 81 EC ? ? 00 00 48 8B 05

			// 2026/5/11 @zetsr
			// push rbp; push r13; push r15; sub rsp, 0x40; mov rax, [rdx+0x20]; mov r11, [r8]
			// 40 55 41 55 41 57 48 83 EC ? 48 8B ? ? 49 8B

			// 2026/5/20 @zetsr
			// mov r11, rsp | push rbp | push r14 | push r15 | sub rsp, alloc | mov rax, [rip+offset]
			// 4C 8B DC 55 41 56 41 57 48 81 EC 80 08 00 00 48 8B 05 CA 9B D4 08

			// 2026/5/23 @zetsr
			// push rbx | push rbp | push rsi | push rdi | push r12 | push r14 | push r15 | sub rsp, alloc | mov rax, [rip+offset] | xor rax, rsp | mov [rsp+offset], rax | mov r14, rcx | mov r15, r8 | lea rcx, [rsp+offset]
			// 40 53 55 56 57 41 54 41 56 41 57 48 81 EC 60 08 00 00 48 8B 05 27 B8 AC 08 48 33 C4 48 89 84 24 50 08 00 00 4C 8B F1 4D 8B F8 48 8D 4C 24 30

			// 2026/6/6 @zetsr
			// mov r11, rsp | push rbp | push r14 | push r15 | sub rsp, alloc | mov rax
			// 4C 8B DC 55 41 56 41 57 48 81 EC 80 08 00 00 48 8B 05

			// 2026/6/10 @zetsr
			// push rbx | push rbp | push rsi | push rdi | push r12 | push r14 | push r15 | sub rsp, alloc | mov rax, [rip+offset] | xor rax, rsp | mov [rsp+offset], rax | mov r14, rcx
			std::string Tick = "4C 8B DC 55 41 56 41 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? 49";
		}
		*/

		namespace UGameViewportClient {
			// https://github.com/vsylva/ark-asa-internal
			// VTable = 122，不知道这个虚表会不会过期，可能还是需要做特征码

			// 8B C2 35 DE C5 F3 59 44 8B C0 69 C0 B1 79 37 9E 41 C1 E8 0D 44 33 C0 41 69 C0 6B CA EB 85
			// mov eax, edx | xor eax, imm32 | mov r8d, eax | imul eax, eax, imm32 | shr r8d, imm8 | xor r8d, eax | imul r8d, r8d, imm32
			
			// 122
			inline std::string PostRender = _XOR_("8B C2 35 ? ? ? ? 44").str();
		}

		namespace UConsole {
			// 2026/4/3 @zetsr
			// 48 8B C4 ? ? 48 83 EC ? 48 89 58 ? 48 8B DA 2026/4/3 过期了

			// mov [rsp + offset], rbx push rbp push rsi push rdi push r12 push r13 push r14 push r15 mov rax, [rcx] sub rsp, offset movsxd rax, [rdx + offset] xor r14d, r14d mov rax, [rcx]
			// 48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8B ? 48 83 EC ? 48 63 ? ? 45 33 F6 48 8B

			// 2026/5/11 @zetsr
			// mov rax, rsp; push r12; sub rsp, 0x60; mov [rax+0x08], rbx; mov rbx, rdx
			// 48 8B C4 41 54 48 83 EC ? 48 89 58 ? 48 8B ?

			// 2026/5/20 @zetsr
			// mov [rsp+8], rbx | push rbp | push rsi | push rdi | push r12 | push r13 | push r14 | push r15 | mov rbp, rsp | sub rsp, alloc | movsxd rsi, [rdx+offset] | xor r14d, r14d
			// 48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 40 48 63 72 08 45 33 F6

			// 2026/5/23 @zetsr
			// mov rax, rsp | push r12 | sub rsp, alloc | mov [rax+8], rbx | mov rbx, rdx
			// 48 8B C4 41 54 48 83 EC 60 48 89 58 08 48 8B DA

			// 2026/6/6 @zetsr
			// mov [rsp+8], rbx | push rbp | push rsi | push rdi | push r12 | push r13 | push r14 | push r15 | mov rbp, rsp | sub rsp, alloc | movsxd rsi, [rdx+offset]
			// 48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 40 48 63 72 08

			// 2026/6/10 @zetsr
			// mov rax, rsp | push r12 | sub rsp, alloc | mov [rax+8], rbx | mov

			// 99
			inline std::string OutputTextLine = _XOR_("48 8B C4 41 54 48 83 EC ? 48 89 58 ? 48").str();
		}
	}
}