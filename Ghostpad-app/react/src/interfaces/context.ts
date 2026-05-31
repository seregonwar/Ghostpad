import {
  CommandProps,
  LoadedProjectProps,
  UnloadedProjectProps,
  UnsignedUserProps,
  SignedUserProps,
} from ".";
export interface ContextProps {
  app: {
    isLoading: boolean;
  };
  user: UnsignedUserProps | SignedUserProps;
  project: UnloadedProjectProps | LoadedProjectProps;
  network: {
    isConnected: boolean;
    isSearching: boolean;
    ip?: string;
    port?: number;
    activeConsoleId?: string;
  };
  usb:
    | {
        isConnected: false;
        isSearching: boolean;
        device?: USBDevice;
      }
    | {
        isConnected: true;
        isSearching: boolean;
        device: USBDevice;
      };
  bluetooth:
    | {
        isConnected: false;
        characteristic?: BluetoothRemoteGATTCharacteristic;
        device?: BluetoothDevice;
      }
    | {
        isConnected: true;
        characteristic: BluetoothRemoteGATTCharacteristic;
        device: BluetoothDevice;
      };
  media:
    | {
        isConnected: false;
        activeDeviceId?: string;
        devices?: MediaDeviceInfo[];
        stream?: MediaStream;
        recorder?: MediaRecorder;
      }
    | {
        isConnected: true;
        activeDeviceId?: string;
        devices?: MediaDeviceInfo[];
        stream?: MediaStream;
        recorder?: MediaRecorder;
      };
  emulator: {
    state: "standby" | "recording" | "playing" | "repeating";
    time: number;
    recordingStartedAt?: number;
    command: CommandProps;
    saveTo?: "db" | "storage";
  };
  gamePad: {
    isConnected: boolean;
    buttonStates: {
      [key: number]: boolean | number;
    };
    stickStates: {
      [key: number]: number;
    };
  };
}
