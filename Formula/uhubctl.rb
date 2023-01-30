class Uhubctl < Formula
  desc "USB hub per-port power control"
  homepage "https://github.com/mvp/uhubctl"
  head "https://github.com/mvp/uhubctl.git"
  url "https://github.com/mvp/uhubctl/archive/v2.5.0.tar.gz"
  sha256 "7be75781b709c36c03c68555f06347d70e5f4e8fd2d17fd481f20626fb4c6038"
  license "GPL-2.0"

  depends_on "libusb"
  depends_on "pkg-config" => :build

  livecheck do
    url :stable
  end

  def install
    system "make"
    bin.install "uhubctl"
  end

  test do
    system "#{bin}/uhubctl", "-v"
  end
end
