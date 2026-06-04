import React from "react";
// Hooks
import { Context } from "../../../hooks/Provider";
// Styles
import styled from "styled-components";
import * as Layout from "../../../styles/Layout";
// Components
import { GamePad } from "./GamePad";
import { CommandTable } from "./CommandTable";

export const Viewer: React.FC = () => {
  const [context] = React.useContext(Context);

  return React.useMemo(() => {
    return (
      <Wrapper>
        <PreviewWrapper>
          <GamePad showSmall={context.media.isConnected} />
        </PreviewWrapper>
        <CommandTable />
      </Wrapper>
    );
  }, [context.media.isConnected]);
};

const Wrapper = styled.div`
  ${Layout.alignElements("inline-flex", "space-between", "space-between")};
  flex-direction: column;
  position: relative;
  width: 100%;
  height: 100%;
`;

const PreviewWrapper = styled.div`
  ${Layout.alignElements("inline-flex", "space-between", "space-between")};
  position: relative;
  width: 100%;
  height: 100%;
`;
