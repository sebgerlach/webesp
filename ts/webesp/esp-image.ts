import { BinFilePartion } from './partition';
import { NVSPartition } from './../nvs/nvs-partition';
import { Terminal } from './terminal';

export class ESPImage {
  partitions: Array<Partition> = [];

  constructor(private readonly terminal:Terminal) { }

  async load() {
    this.partitions.push(new NVSPartition(0x9000, 'NVS Partition', 0x6000));
    this.partitions.push(new BinFilePartion(0x8000, 'bin/partition-table.bin', this.terminal));
    this.partitions.push(new BinFilePartion(0x1000, 'bin/bootloader.bin', this.terminal));
    this.partitions.push(new BinFilePartion(0x10000, 'bin/app.bin', this.terminal));

    for (let i = 0; i < this.partitions.length; ++i) {
      await this.partitions[i].load();
    }
  }

}

