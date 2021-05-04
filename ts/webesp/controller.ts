import { PortController } from './port-controller';
import { Terminal } from './terminal';
import { CommandWriter } from './command-writer';
import { ESPLoader } from './esp-loader';
import { ESPImage } from './esp-image';
import { NVSPartition } from '../nvs/nvs-partition';

const DEFAULT_BAUD = 115200;

export class Controller implements StreamTarget {
  terminal: Terminal;
  connect: HTMLButtonElement;
  reset: HTMLButtonElement;
  port: PortController | null = null;

  constructor() {
    this.connect = <HTMLButtonElement>document.getElementById('connect');
    this.connect.addEventListener('click', this.onConnect.bind(this));
    this.reset = <HTMLButtonElement>document.getElementById('reset');
    this.reset.addEventListener('click', this.onReset.bind(this));
    this.terminal = new Terminal('main');
    if ('serial' in navigator) {
      this.connect.style.display = 'inline';
      document.getElementById('nowebserial')!.style.display = 'none';
    }
  }

  async onConnect() {
    if (this.port !== null) {
      await this.port.disconnect();
      this.port = null;
      this.connect.innerText = 'Select device';
      this.reset.style.display = 'none';
    } else {
      try {
        const p = await navigator.serial.requestPort();
        this.port = new PortController(p, this, this.terminal);
        await this.port.connect(DEFAULT_BAUD);
        this.connect.innerText = 'Disconnect';
        this.reset.style.display = 'inline';
      } catch (err) {
        console.log('Port selection failed: ', err);
      }
    }
  }

  async onReset() {
    this.terminal.print('Reset');
    await this.port!.resetPulse();
  }

  inFrame = false;
  frame = new CommandWriter();
  decoder = new TextDecoder();
  loader: ESPLoader | null = null;
  receive(data: Uint8Array) {
    //inputBuffer.push(...value);
    for (let i = 0; i < data.length; ++i) {
      // Parse the value.
      if (this.inFrame) {
        if (data[i] == 0xc0) {
          this.inFrame = false;
          if (this.loader == null) {
            console.log('Received unexpected frame', data);
          } else {
            this.loader!.receiveFrame(this.frame.unframed());
          }
        } else {
          this.frame.u8(data[i]);
        }
      } else if (data[i] == 0xc0) {
        // This may be the beginning of a framed packet.
        this.inFrame = true;
        this.frame.clear();
      } else if (data[i] == 13) {
        const str = this.decoder.decode(this.frame.get());
        this.terminal.write(str);
        this.frame.clear();
        if (str == 'waiting for download') {
          this.load();
        }
      } else if (data[i] != 10) {
        this.frame.u8(data[i]);
      }
    }
  }

  async load() {
    console.log('Starting sync');
    const image = new ESPImage(this.terminal);
    NVSPartition.entries.push(<HTMLInputElement>document.getElementById('wifiSSID'));
    NVSPartition.entries.push(<HTMLInputElement>document.getElementById('wifiPassword'));
    NVSPartition.entries.push(<HTMLInputElement>document.getElementById('message'));
    await image.load();
    this.loader = new ESPLoader(this.port!, image, this.terminal);
    await this.loader.sync();
  }

}