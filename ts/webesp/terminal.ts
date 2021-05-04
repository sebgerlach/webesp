const sequences : {[key:string]:string}= {
  '[0;32m': '<span style="color:#0c0;">',
  '[0;33m': '<span style="color:#cc0;">',
  '[0m': '</span>',
}

export class Terminal {
  div: HTMLDivElement;
  static readonly maxLogs: number = 200;
  constructor(id: string) {
    this.div = <HTMLDivElement>document.getElementById(id);
  }

  write(a: string) {
    let s = "";
    const entry = this.getLogEntryElement();
    for (let i = 0; i < a.length; ++i) {
      // This is kind of crappy, since it assumes that everything is kind of well formed.
      if (a.charCodeAt(i) == 27) {
        for (const k in sequences) {
          if (a.substr(i + 1, k.length) == k) {
            s += sequences[k];
            i += k.length;
          }
        }
      } else {
        s += a[i];
      }
    }
    entry.innerHTML = s;
    this.div.appendChild(entry);
    this.truncateTerminal();
    this.scrollTerminal();
  }

  print(s: string) {
    const entry = this.getLogEntryElement();
    entry.innerHTML += '<span style="background-color:#444;border:1px solid #ccc; border-radius:3px;">' + s + '</span>';
    this.div.appendChild(entry);
    this.truncateTerminal();
    this.scrollTerminal();
  }

  private getLogEntryElement(): HTMLSpanElement {
    const entry = document.createElement('div');
    entry.classList.add('log-entry');
    return entry;
  }

  private truncateTerminal() {
    while (this.div.children.length > Terminal.maxLogs) {
      this.div.removeChild(this.div.firstChild as Node);
    }
  }

  private scrollTerminal() {
    this.div.scrollTop = this.div.scrollHeight;
  }
}