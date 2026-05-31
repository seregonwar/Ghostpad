import React from "react";
import styled from "styled-components";

export const CapturePreview: React.FC = () => {
  const videoRef = React.useRef<HTMLVideoElement>(null);
  const streamRef = React.useRef<MediaStream | null>(null);
  const [error, setError] = React.useState<string>();
  const [resolution, setResolution] = React.useState("");

  const params = React.useMemo(() => new URLSearchParams(window.location.search), []);
  const deviceId = params.get("deviceId") || "";
  const label = params.get("label") || "Capture Device";

  React.useEffect(() => {
    if (!deviceId) {
      setError("No capture device specified.");
      return;
    }

    let active = true;

    const start = async () => {
      try {
        const stream = await navigator.mediaDevices.getUserMedia({
          video: {
            deviceId: { exact: deviceId },
            width: { ideal: 3840, max: 7680 },
            height: { ideal: 2160, max: 4320 },
            frameRate: { ideal: 60, max: 120 },
          },
        });
        if (!active) {
          stream.getTracks().forEach((t) => t.stop());
          return;
        }
        streamRef.current = stream;
        if (videoRef.current) {
          videoRef.current.srcObject = stream;
          await videoRef.current.play().catch(() => undefined);
        }
        const track = stream.getVideoTracks()[0];
        const settings = track?.getSettings();
        if (settings?.width && settings?.height) {
          setResolution(`${settings.width}×${settings.height}`);
        }
      } catch (err) {
        setError(err instanceof Error ? err.message : "Failed to open capture device.");
      }
    };

    start();

    return () => {
      active = false;
      streamRef.current?.getTracks().forEach((t) => t.stop());
      streamRef.current = null;
    };
  }, [deviceId]);

  if (error) {
    return (
      <Message>
        <h2>Capture Error</h2>
        <p>{error}</p>
      </Message>
    );
  }

  return (
    <Page>
      <Bar>
        <span>{label}</span>
        {resolution && <span>{resolution}</span>}
      </Bar>
      <VideoWrap>
        <Video ref={videoRef} autoPlay muted playsInline />
      </VideoWrap>
    </Page>
  );
};

const Page = styled.div`
  width: 100vw;
  height: 100vh;
  background: #000;
  display: flex;
  flex-direction: column;
  overflow: hidden;
`;

const Bar = styled.div`
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 6px 12px;
  background: #111;
  color: #eee;
  font-size: 13px;
  flex-shrink: 0;
`;

const VideoWrap = styled.div`
  flex: 1;
  display: grid;
  place-items: center;
  min-height: 0;
`;

const Video = styled.video`
  max-width: 100%;
  max-height: 100%;
  width: auto;
  height: auto;
  object-fit: contain;
`;

const Message = styled.div`
  width: 100vw;
  height: 100vh;
  display: grid;
  place-items: center;
  background: #111;
  color: #eee;
  text-align: center;
  p {
    opacity: 0.8;
  }
`;
