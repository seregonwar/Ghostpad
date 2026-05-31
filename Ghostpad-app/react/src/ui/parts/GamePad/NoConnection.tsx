import React from "react";
import styled from "styled-components";
import * as Layout from "../../../styles/Layout";
import ImagePath from "../../../assets/masterhand.png";

export const NoConnection: React.FC = () => {
  return (
    <Wrapper>
      <Image src={ImagePath} alt="Ghostpad controller" />
      <b>
        <span className="material-icon">info</span>Waiting for Connection
      </b>
      <span>
        Connect a <b>DualSense or compatible controller</b> to your PC via USB
        or Bluetooth, then press any button.
        <br />
        Also connect to your PS5 from the network menu above.
      </span>
    </Wrapper>
  );
};

export default NoConnection;

const Wrapper = styled.main`
  ${Layout.alignElements("flex", "center", "center")};
  ${Layout.spacingBetweenElements("vertical", 1)};
  flex-direction: column;
  text-align: center;
  height: 100%;
  > b {
    font-size: 22px;
    font-weight: bold;
    display: inline-flex;
    justify-content: center;
    align-items: center;
    > span {
      margin-right: 8px;
    }
  }
`;

const Image = styled.img`
  padding-bottom: 24px;
`;
