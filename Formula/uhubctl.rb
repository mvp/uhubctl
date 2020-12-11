class Uhubctl < Formula
  desc "USB hub per-port power control"
  homepage "https://github.com/mvp/uhubctl"
  head "https://github.com/mvp/uhubctl.git"
  url "https://github.com/mvp/uhubctl/archive/v2.2.0.tar.gz"
  sha256 "e5a722cb41967903bedbab4eea566ab332241a7f05fc7bc9c386b9a5e1762d8b"
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
