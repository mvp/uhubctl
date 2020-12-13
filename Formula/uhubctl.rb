class Uhubctl < Formula
  desc "USB hub per-port power control"
  homepage "https://github.com/mvp/uhubctl"
  head "https://github.com/mvp/uhubctl.git"
  url "https://github.com/mvp/uhubctl/archive/v2.3.0.tar.gz"
  sha256 "714f733592d3cb6ba8efc84fbc03b1beed2323918ff33aef01cdb956755be7b7"
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
