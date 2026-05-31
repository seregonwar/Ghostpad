import React from "react";
import styled from "styled-components";
import { resolveProjectImageUrl } from "../../utils/resolveProjectImageUrl";
import { Colors } from "../../styles/Colors";

export interface ProjectThumbnailProps {
  imageUrl?: string;
  alt?: string;
  size?: number;
  fill?: boolean;
  className?: string;
}

export const ProjectThumbnail: React.FC<ProjectThumbnailProps> = ({
  imageUrl,
  alt = "",
  size = 28,
  fill = false,
  className,
}) => {
  const [failed, setFailed] = React.useState(false);
  const resolved = resolveProjectImageUrl(imageUrl);

  React.useEffect(() => {
    setFailed(false);
  }, [imageUrl]);

  if (!resolved || failed) {
    return (
      <Placeholder size={size} fill={fill} className={className} aria-hidden>
        <span className="material-icon">sports_esports</span>
      </Placeholder>
    );
  }

  return (
    <Img
      size={size}
      fill={fill}
      className={className}
      src={resolved}
      alt={alt}
      onError={() => setFailed(true)}
    />
  );
};

const Placeholder = styled.span<{ size: number; fill?: boolean }>`
  ${(p) =>
    p.fill
      ? `
    position: absolute;
    inset: 0;
    width: 100%;
    height: 100%;
  `
      : `
    width: ${p.size}px;
    height: ${p.size}px;
  `}
  flex-shrink: 0;
  display: grid;
  place-items: center;
  border-radius: 2px;
  background: ${Colors.bgColorLv2};
  color: ${Colors.elementColorWeak};
  .material-icon {
    font-size: ${(p) => (p.fill ? 40 : Math.round(p.size * 0.65))}px;
    opacity: 0.45;
  }
`;

const Img = styled.img<{ size: number; fill?: boolean }>`
  ${(p) =>
    p.fill
      ? `
    position: absolute;
    inset: 0;
    width: 100%;
    height: 100%;
  `
      : `
    width: ${p.size}px;
    height: ${p.size}px;
  `}
  flex-shrink: 0;
  object-fit: cover;
  object-position: center;
  border-radius: 2px;
`;
