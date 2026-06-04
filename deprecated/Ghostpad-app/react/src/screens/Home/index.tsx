import React from "react";
import { Link } from "react-router-dom";
import TheHeader from "../../ui/systems/Navigation/TheHeader";
import styled, { keyframes } from "styled-components";
import "@fontsource/oxanium/700.css";
import "@fontsource/oxanium/800.css";
import { Colors } from "../../styles/Colors";

const IconConsoles = () => (
  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.75" strokeLinecap="round" strokeLinejoin="round">
    <rect x="2" y="3" width="20" height="14" rx="2"/>
    <line x1="8" y1="21" x2="16" y2="21"/>
    <line x1="12" y1="17" x2="12" y2="21"/>
  </svg>
);

const IconInput = () => (
  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.75" strokeLinecap="round" strokeLinejoin="round">
    <path d="M6 12h4m-2-2v4"/>
    <circle cx="16.5" cy="11" r="0.75" fill="currentColor" stroke="none"/>
    <circle cx="19.5" cy="11" r="0.75" fill="currentColor" stroke="none"/>
    <path d="M15.5 5.5h-7a4 4 0 0 0-4 4v3a4 4 0 0 0 4 4h7a4 4 0 0 0 4-4v-3a4 4 0 0 0-4-4z"/>
  </svg>
);

const IconProjects = () => (
  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.75" strokeLinecap="round" strokeLinejoin="round">
    <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/>
  </svg>
);

const IconSettings = () => (
  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.75" strokeLinecap="round" strokeLinejoin="round">
    <circle cx="12" cy="12" r="3"/>
    <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/>
  </svg>
);

const QUICK_LINKS = [
  {
    to: "/consoles",
    Icon: IconConsoles,
    title: "Manage Consoles",
    desc: "Add, connect, and manage PS5 consoles on your network",
    accent: Colors.brandColorPrimary,
  },
  {
    to: "/input",
    Icon: IconInput,
    title: "Input Redirection",
    desc: "Stream keyboard, mouse, and gamepad input live to PS5",
    accent: Colors.elementColorLink,
  },
  {
    to: "/projects",
    Icon: IconProjects,
    title: "Projects",
    desc: "Browse and run macro scripts and automation projects",
    accent: Colors.statusColorSuccess,
  },
  {
    to: "/settings",
    Icon: IconSettings,
    title: "Settings",
    desc: "Configure Ghostpad payload connection and behavior",
    accent: Colors.brandColorSecondary,
  },
];

export const Home = () => {
  return (
    <>
      <TheHeader title="Ghostpad" buttonUrl="https://github.com/StonedModder" />
      <Hero>
        <HeroBg />
        <HeroGrid />
        <HeroContent>
          <Eyebrow>made by StonedModder</Eyebrow>
          <HeroTitle>GHOSTPAD</HeroTitle>
          <HeroDesc>
            Control and automate your PlayStation 5 over your local network.
            <br />
            Macros, recording, and virtual DualSense input.
          </HeroDesc>
          <HeroCTAs>
            <Link to="/consoles">
              <PrimaryBtn>Connect to PS5</PrimaryBtn>
            </Link>
            <Link to="/input">
              <OutlineBtn>Input Redirection</OutlineBtn>
            </Link>
          </HeroCTAs>
        </HeroContent>
        <HeroDecor aria-hidden="true">
          <DecorRing $size={320} $opacity={0.06} />
          <DecorRing $size={220} $opacity={0.1} />
          <DecorRing $size={120} $opacity={0.16} />
          <DecorDot />
        </HeroDecor>
      </Hero>

      <CardsSection>
        <SectionLabel>QUICK ACCESS</SectionLabel>
        <CardsGrid>
          {QUICK_LINKS.map((link) => (
            <CardLink key={link.to} to={link.to} $accent={link.accent}>
              <CardIconWrap $accent={link.accent}>
                <link.Icon />
              </CardIconWrap>
              <CardText>
                <CardTitle>{link.title}</CardTitle>
                <CardDesc>{link.desc}</CardDesc>
              </CardText>
              <CardArrow $accent={link.accent}>→</CardArrow>
            </CardLink>
          ))}
        </CardsGrid>
      </CardsSection>
    </>
  );
};

// ─── Animations ──────────────────────────────────────────────────────────────

const pulseRing = keyframes`
  0%, 100% { transform: scale(1); opacity: 1; }
  50% { transform: scale(1.04); opacity: 0.7; }
`;

const fadeUp = keyframes`
  from { opacity: 0; transform: translateY(16px); }
  to   { opacity: 1; transform: translateY(0); }
`;

// ─── Hero ─────────────────────────────────────────────────────────────────────

const Hero = styled.section`
  position: relative;
  margin-top: 60px;
  min-height: 520px;
  display: flex;
  align-items: center;
  overflow: hidden;
  padding: 80px 48px 80px;

  @media (max-width: 768px) {
    padding: 56px 28px;
    min-height: unset;
  }

  @media (max-width: 480px) {
    padding: 40px 16px;
  }
`;

const HeroBg = styled.div`
  position: absolute;
  inset: 0;
  background:
    radial-gradient(ellipse 60% 80% at 10% 50%, rgba(155, 81, 224, 0.18) 0%, transparent 70%),
    radial-gradient(ellipse 40% 60% at 80% 20%, rgba(110, 196, 255, 0.07) 0%, transparent 60%),
    ${Colors.bgColorLv0};
  z-index: 0;
`;

const HeroGrid = styled.div`
  position: absolute;
  inset: 0;
  background-image:
    linear-gradient(rgba(155, 81, 224, 0.04) 1px, transparent 1px),
    linear-gradient(90deg, rgba(155, 81, 224, 0.04) 1px, transparent 1px);
  background-size: 48px 48px;
  z-index: 0;
`;

const HeroContent = styled.div`
  position: relative;
  z-index: 1;
  max-width: 580px;
  animation: ${fadeUp} 0.6s ease both;
`;

const Eyebrow = styled.div`
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.18em;
  color: ${Colors.brandColorPrimary};
  text-transform: uppercase;
  margin-bottom: 20px;
  display: flex;
  align-items: center;
  gap: 10px;

  &::before {
    content: "";
    display: block;
    width: 24px;
    height: 2px;
    background: ${Colors.brandColorPrimary};
    border-radius: 1px;
  }
`;

const HeroTitle = styled.h1`
  font-family: "Oxanium", "Poppins", sans-serif;
  font-size: clamp(52px, 8vw, 96px);
  font-weight: 800;
  letter-spacing: 0.04em;
  line-height: 1;
  color: ${Colors.elementColorInverse};
  margin-bottom: 24px;
  animation: ${fadeUp} 0.6s 0.08s ease both;
`;

const HeroDesc = styled.p`
  font-size: 15px;
  line-height: 1.7;
  color: ${Colors.elementColorWeak};
  margin-bottom: 36px;
  animation: ${fadeUp} 0.6s 0.16s ease both;
`;

const HeroCTAs = styled.div`
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
  animation: ${fadeUp} 0.6s 0.24s ease both;

  a {
    text-decoration: none;
  }
`;

const PrimaryBtn = styled.button`
  height: 44px;
  padding: 0 28px;
  border-radius: 6px;
  border: none;
  background: ${Colors.brandColorPrimary};
  color: #fff;
  font-size: 14px;
  font-weight: 600;
  cursor: pointer;
  transition: opacity 0.15s, transform 0.15s;

  &:hover {
    opacity: 0.88;
    transform: translateY(-1px);
  }
`;

const OutlineBtn = styled.button`
  height: 44px;
  padding: 0 28px;
  border-radius: 6px;
  border: 1px solid ${Colors.borderColorLv2};
  background: transparent;
  color: ${Colors.elementColorWeak};
  font-size: 14px;
  font-weight: 600;
  cursor: pointer;
  transition: border-color 0.15s, color 0.15s, transform 0.15s;

  &:hover {
    border-color: ${Colors.brandColorPrimary};
    color: ${Colors.brandColorPrimary};
    transform: translateY(-1px);
  }
`;

// ─── Hero decorative rings (right side) ──────────────────────────────────────

const HeroDecor = styled.div`
  position: absolute;
  right: 10%;
  top: 50%;
  transform: translateY(-50%);
  z-index: 0;
  display: flex;
  align-items: center;
  justify-content: center;

  @media (max-width: 700px) {
    display: none;
  }
`;

const DecorRing = styled.div<{ $size: number; $opacity: number }>`
  position: absolute;
  width: ${({ $size }) => $size}px;
  height: ${({ $size }) => $size}px;
  border-radius: 50%;
  border: 1.5px solid rgba(155, 81, 224, ${({ $opacity }) => $opacity});
  animation: ${pulseRing} ${({ $size }) => 3 + $size / 200}s ease-in-out infinite;
`;

const DecorDot = styled.div`
  width: 14px;
  height: 14px;
  border-radius: 50%;
  background: ${Colors.brandColorPrimary};
  box-shadow: 0 0 20px rgba(155, 81, 224, 0.8), 0 0 40px rgba(155, 81, 224, 0.4);
`;

// ─── Cards ────────────────────────────────────────────────────────────────────

const CardsSection = styled.div`
  padding: 0 48px 64px;
  max-width: 1200px;
  width: 100%;

  @media (max-width: 768px) {
    padding: 0 28px 48px;
  }

  @media (max-width: 480px) {
    padding: 0 16px 32px;
  }
`;

const SectionLabel = styled.div`
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.14em;
  color: ${Colors.elementColorMute};
  text-transform: uppercase;
  margin-bottom: 20px;
`;

const CardsGrid = styled.div`
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
  gap: 12px;
`;

const CardLink = styled(Link)<{ $accent: string }>`
  display: flex;
  align-items: center;
  gap: 16px;
  padding: 20px 20px;
  border-radius: 10px;
  border: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv1};
  text-decoration: none;
  cursor: pointer;
  transition: border-color 0.18s, background 0.18s, transform 0.18s, box-shadow 0.18s;
  position: relative;
  overflow: hidden;

  &::before {
    content: "";
    position: absolute;
    left: 0;
    top: 0;
    bottom: 0;
    width: 3px;
    background: ${({ $accent }) => $accent};
    opacity: 0;
    transition: opacity 0.18s;
    border-radius: 10px 0 0 10px;
  }

  &:hover {
    border-color: ${({ $accent }) => $accent}55;
    background: ${Colors.bgColorLv2};
    transform: translateY(-2px);
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.3);

    &::before {
      opacity: 1;
    }
  }
`;

const CardIconWrap = styled.div<{ $accent: string }>`
  flex-shrink: 0;
  width: 44px;
  height: 44px;
  border-radius: 10px;
  background: ${({ $accent }) => $accent}18;
  border: 1px solid ${({ $accent }) => $accent}30;
  display: flex;
  align-items: center;
  justify-content: center;
  color: ${({ $accent }) => $accent};

  svg {
    width: 20px;
    height: 20px;
  }
`;

const CardText = styled.div`
  flex: 1;
  min-width: 0;
`;

const CardTitle = styled.div`
  font-size: 14px;
  font-weight: 600;
  color: ${Colors.elementColorDefault};
  margin-bottom: 3px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
`;

const CardDesc = styled.div`
  font-size: 12px;
  color: ${Colors.elementColorWeak};
  line-height: 1.5;
`;

const CardArrow = styled.div<{ $accent: string }>`
  flex-shrink: 0;
  font-size: 18px;
  color: ${Colors.elementColorMute};
  transition: color 0.18s, transform 0.18s;

  ${CardLink}:hover & {
    color: ${({ $accent }) => $accent};
    transform: translateX(3px);
  }
`;
