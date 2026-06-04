import React from "react";
import { Link } from "react-router-dom";
import styled from "styled-components";
import Colors from "../../../styles/Colors";
import { alignElements } from "../../../styles/Layout";

import logo from "../../../assets/icon.png";
import GitHubButton from "../../parts/Button/GitHubButton";

interface TheHeaderProps {
  title: string;
  buttonUrl: string;
}

export const TheHeader = (props: TheHeaderProps) => {
  return (
    <Header>
      <Logo to="/">{props.title}</Logo>
      <HeaderRight>
        <HomeLink to="/" title="Home">
          <span className="material-icon">home</span>
        </HomeLink>
        <XButton href="https://x.com/StonedModder" target="_blank" rel="noopener noreferrer" title="View on X">
          <XLogo aria-hidden="true">
            <svg viewBox="0 0 24 24" fill="currentColor" xmlns="http://www.w3.org/2000/svg">
              <path d="M18.244 2.25h3.308l-7.227 8.26 8.502 11.24H16.17l-4.714-6.231-5.401 6.231H2.746l7.73-8.835L1.254 2.25H8.08l4.713 6.231 5.45-6.231zm-1.161 17.52h1.833L7.084 4.126H5.117z"/>
            </svg>
          </XLogo>
          <XButtonLabel>View on X</XButtonLabel>
        </XButton>
        <GitHubButton url={props.buttonUrl} />
      </HeaderRight>
    </Header>
  );
};

const Header = styled.header`
  position: fixed;
  z-index: 10;
  top: 0;
  height: 60px;
  width: 100vw;
  padding: 0 16px;
  ${alignElements("flex", "space-between", "center")};
  background: ${Colors.bgColorLv0};
  border-bottom: 1px solid ${Colors.borderColorLv1};
`;

const HeaderRight = styled.div`
  display: flex;
  align-items: center;
  gap: 10px;
`;

const HomeLink = styled(Link)`
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  border-radius: 6px;
  border: 1px solid ${Colors.borderColorLv2};
  background: ${Colors.bgColorLv1};
  color: ${Colors.elementColorWeak};
  text-decoration: none;
  transition: border-color 0.15s, color 0.15s, background 0.15s;

  .material-icon {
    font-size: 18px;
  }

  &:hover {
    border-color: ${Colors.brandColorPrimary};
    color: ${Colors.brandColorPrimary};
    background: ${Colors.bgColorLv2};
  }
`;

const XButton = styled.a`
  display: inline-flex;
  align-items: center;
  gap: 6px;
  height: 32px;
  padding: 0 12px;
  border-radius: 6px;
  border: 1px solid ${Colors.borderColorLv2};
  background: ${Colors.bgColorLv1};
  color: ${Colors.elementColorWeak};
  font-size: 12px;
  font-weight: 600;
  text-decoration: none;
  white-space: nowrap;
  transition: border-color 0.15s, color 0.15s, background 0.15s;

  &:hover {
    border-color: ${Colors.elementColorDefault};
    color: ${Colors.elementColorDefault};
    background: ${Colors.bgColorLv2};
  }

  @media (max-width: 480px) {
    padding: 0 8px;
  }
`;

const XLogo = styled.span`
  display: flex;
  align-items: center;
  svg {
    width: 13px;
    height: 13px;
  }
`;

const XButtonLabel = styled.span`
  @media (max-width: 480px) {
    display: none;
  }
`;

interface LogoProps {
  logo?: string;
}
const Logo = styled(Link)<LogoProps>`
  font-weight: 600;
  padding-left: 40px;
  height: 30px;
  position: relative;
  line-height: 30px;
  font-size: 24px;
  color: ${Colors.brandColorPrimary};
  &::before {
    content: "";
    position: absolute;
    left: 0;
    height: 30px;
    width: 30px;
    background: url(${logo}) no-repeat left center / contain;
  }
`;

export default TheHeader;
